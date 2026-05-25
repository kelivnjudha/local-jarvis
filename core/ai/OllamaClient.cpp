#include "OllamaClient.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace local_jarvis::ai {
namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

bool ensureSocketsInitialized()
{
    static const bool initialized = []() {
        WSADATA data {};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return initialized;
}

void closeSocket(SocketHandle socket)
{
    closesocket(socket);
}

#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

bool ensureSocketsInitialized()
{
    return true;
}

void closeSocket(SocketHandle socket)
{
    close(socket);
}
#endif

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool sendAll(SocketHandle socket, const std::string &data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int result = send(socket, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (result <= 0) {
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

void setSocketTimeouts(SocketHandle socket, int timeoutMs)
{
#if defined(_WIN32)
    const DWORD timeout = static_cast<DWORD>(timeoutMs);
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
    timeval timeout {};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

std::string readAll(SocketHandle socket)
{
    std::string data;
    char buffer[4096] {};

    while (true) {
        const int result = recv(socket, buffer, sizeof(buffer), 0);
        if (result <= 0) {
            break;
        }

        data.append(buffer, static_cast<std::size_t>(result));
    }

    return data;
}

std::string decodeChunkedBody(const std::string &body)
{
    std::string decoded;
    std::size_t offset = 0;

    while (offset < body.size()) {
        const std::size_t lineEnd = body.find("\r\n", offset);
        if (lineEnd == std::string::npos) {
            break;
        }

        const std::string sizeText = body.substr(offset, lineEnd - offset);
        std::size_t chunkSize = 0;
        try {
            chunkSize = std::stoul(sizeText, nullptr, 16);
        } catch (...) {
            break;
        }
        if (chunkSize == 0) {
            break;
        }

        offset = lineEnd + 2;
        if (offset + chunkSize > body.size()) {
            break;
        }

        decoded.append(body, offset, chunkSize);
        offset += chunkSize + 2;
    }

    return decoded;
}

std::string trim(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character);
    }).base();

    if (first >= last) {
        return {};
    }

    return { first, last };
}

} // namespace

OllamaClient::OllamaClient(std::string host, int port, int timeoutMs)
    : m_host(std::move(host))
    , m_port(port)
    , m_timeoutMs(timeoutMs)
{
}

bool OllamaClient::isOllamaRunning()
{
    HttpResponse response;
    if (!request("GET", "/api/tags", {}, response)) {
        return false;
    }

    if (response.statusCode != 200) {
        setLastError("Ollama returned HTTP " + std::to_string(response.statusCode));
        return false;
    }

    m_lastError.clear();
    return true;
}

bool OllamaClient::isModelInstalled(const std::string &modelName)
{
    const auto models = listLocalModels();
    return std::find(models.begin(), models.end(), modelName) != models.end();
}

std::vector<std::string> OllamaClient::listLocalModels()
{
    HttpResponse response;
    if (!request("GET", "/api/tags", {}, response)) {
        return {};
    }

    if (response.statusCode != 200) {
        setLastError("Ollama returned HTTP " + std::to_string(response.statusCode));
        return {};
    }

    const auto json = nlohmann::json::parse(response.body, nullptr, false);
    if (json.is_discarded() || !json.is_object() || !json.contains("models") || !json["models"].is_array()) {
        setLastError("Ollama tags response was not valid JSON.");
        return {};
    }

    std::vector<std::string> models;
    for (const auto &model : json["models"]) {
        if (model.is_object() && model.contains("name") && model["name"].is_string()) {
            models.push_back(model["name"].get<std::string>());
        }
    }

    m_lastError.clear();
    return models;
}

bool OllamaClient::generate(const std::string &modelName, const std::string &prompt, std::string &output)
{
    const nlohmann::json requestBody = {
        { "model", modelName },
        { "prompt", prompt },
        { "stream", false }
    };

    HttpResponse response;
    if (!request("POST", "/api/generate", requestBody.dump(), response)) {
        return false;
    }

    if (response.statusCode != 200) {
        setLastError("Generate request returned HTTP " + std::to_string(response.statusCode) + ": " + response.body);
        return false;
    }

    const auto json = nlohmann::json::parse(response.body, nullptr, false);
    if (json.is_discarded()
        || !json.is_object()
        || !json.contains("response")
        || !json["response"].is_string()) {
        setLastError("Generate response did not include a response field.");
        return false;
    }

    output = trim(json["response"].get<std::string>());
    m_lastError.clear();
    return true;
}

bool OllamaClient::chat(const std::string &modelName, const std::vector<ChatMessage> &messages, std::string &output)
{
    nlohmann::json messageArray = nlohmann::json::array();
    for (const auto &message : messages) {
        messageArray.push_back({
            { "role", message.role },
            { "content", message.content }
        });
    }

    const nlohmann::json requestBody = {
        { "model", modelName },
        { "messages", messageArray },
        { "stream", false }
    };

    HttpResponse response;
    if (!request("POST", "/api/chat", requestBody.dump(), response)) {
        return false;
    }

    if (response.statusCode != 200) {
        setLastError("Chat request returned HTTP " + std::to_string(response.statusCode) + ": " + response.body);
        return false;
    }

    const auto json = nlohmann::json::parse(response.body, nullptr, false);
    if (json.is_discarded()
        || !json.is_object()
        || !json.contains("message")
        || !json["message"].is_object()
        || !json["message"].contains("content")
        || !json["message"]["content"].is_string()) {
        setLastError("Chat response did not include assistant content.");
        return false;
    }

    output = trim(json["message"]["content"].get<std::string>());
    m_lastError.clear();
    return true;
}

const std::string &OllamaClient::lastError() const
{
    return m_lastError;
}

bool OllamaClient::request(const std::string &method, const std::string &path, const std::string &body, HttpResponse &response)
{
    if (!ensureSocketsInitialized()) {
        setLastError("Failed to initialize socket subsystem.");
        return false;
    }

    addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *addresses = nullptr;
    const std::string portText = std::to_string(m_port);
    if (getaddrinfo(m_host.c_str(), portText.c_str(), &hints, &addresses) != 0) {
        setLastError("Failed to resolve Ollama host.");
        return false;
    }

    SocketHandle socket = kInvalidSocket;
    for (addrinfo *address = addresses; address != nullptr; address = address->ai_next) {
        socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket == kInvalidSocket) {
            continue;
        }

        setSocketTimeouts(socket, m_timeoutMs);
        if (connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) {
            break;
        }

        closeSocket(socket);
        socket = kInvalidSocket;
    }

    freeaddrinfo(addresses);

    if (socket == kInvalidSocket) {
        setLastError("Could not connect to local Ollama at http://" + m_host + ":" + std::to_string(m_port));
        return false;
    }

    std::ostringstream requestStream;
    requestStream << method << ' ' << path << " HTTP/1.1\r\n"
                  << "Host: " << m_host << ':' << m_port << "\r\n"
                  << "Accept: application/json\r\n"
                  << "Connection: close\r\n";
    if (!body.empty()) {
        requestStream << "Content-Type: application/json\r\n"
                      << "Content-Length: " << body.size() << "\r\n";
    }
    requestStream << "\r\n" << body;

    if (!sendAll(socket, requestStream.str())) {
        closeSocket(socket);
        setLastError("Failed to send request to local Ollama.");
        return false;
    }

    const std::string rawResponse = readAll(socket);
    closeSocket(socket);

    const std::size_t headerEnd = rawResponse.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        setLastError("Invalid HTTP response from local Ollama.");
        return false;
    }

    const std::string headers = rawResponse.substr(0, headerEnd);
    response.body = rawResponse.substr(headerEnd + 4);

    std::istringstream headerStream(headers);
    std::string httpVersion;
    headerStream >> httpVersion >> response.statusCode;

    if (lowerCopy(headers).find("transfer-encoding: chunked") != std::string::npos) {
        response.body = decodeChunkedBody(response.body);
    }

    m_lastError.clear();
    return response.statusCode > 0;
}

void OllamaClient::setLastError(std::string message)
{
    m_lastError = std::move(message);
}

} // namespace local_jarvis::ai
