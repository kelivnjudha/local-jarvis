#include "OllamaClient.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <optional>
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

std::string jsonEscape(const std::string &value)
{
    std::ostringstream stream;
    for (const char character : value) {
        switch (character) {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            stream << character;
            break;
        }
    }
    return stream.str();
}

std::string jsonUnescape(const std::string &value)
{
    std::string output;
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\' || index + 1 >= value.size()) {
            output.push_back(value[index]);
            continue;
        }

        const char escaped = value[++index];
        switch (escaped) {
        case 'n':
            output.push_back('\n');
            break;
        case 'r':
            output.push_back('\r');
            break;
        case 't':
            output.push_back('\t');
            break;
        case '"':
        case '\\':
        case '/':
            output.push_back(escaped);
            break;
        default:
            output.push_back(escaped);
            break;
        }
    }

    return output;
}

std::optional<std::string> extractJsonString(const std::string &json, const std::string &key, std::size_t startAt = 0)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t keyIndex = json.find(needle, startAt);
    if (keyIndex == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colonIndex = json.find(':', keyIndex + needle.size());
    if (colonIndex == std::string::npos) {
        return std::nullopt;
    }

    std::size_t quoteIndex = json.find('"', colonIndex + 1);
    if (quoteIndex == std::string::npos) {
        return std::nullopt;
    }

    std::string value;
    bool escaped = false;
    for (std::size_t index = quoteIndex + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (escaped) {
            value.push_back('\\');
            value.push_back(character);
            escaped = false;
            continue;
        }

        if (character == '\\') {
            escaped = true;
            continue;
        }

        if (character == '"') {
            return jsonUnescape(value);
        }

        value.push_back(character);
    }

    return std::nullopt;
}

std::vector<std::string> extractAllJsonStrings(const std::string &json, const std::string &key)
{
    std::vector<std::string> values;
    std::size_t offset = 0;
    const std::string needle = "\"" + key + "\"";

    while (true) {
        const std::size_t keyIndex = json.find(needle, offset);
        if (keyIndex == std::string::npos) {
            break;
        }

        const auto value = extractJsonString(json, key, keyIndex);
        if (value.has_value()) {
            values.push_back(*value);
        }

        offset = keyIndex + needle.size();
    }

    return values;
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
        const std::size_t chunkSize = std::stoul(sizeText, nullptr, 16);
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

    m_lastError.clear();
    return extractAllJsonStrings(response.body, "name");
}

bool OllamaClient::generate(const std::string &modelName, const std::string &prompt, std::string &output)
{
    const std::string body = "{\"model\":\"" + jsonEscape(modelName)
        + "\",\"prompt\":\"" + jsonEscape(prompt)
        + "\",\"stream\":false}";

    HttpResponse response;
    if (!request("POST", "/api/generate", body, response)) {
        return false;
    }

    if (response.statusCode != 200) {
        setLastError("Generate request returned HTTP " + std::to_string(response.statusCode) + ": " + response.body);
        return false;
    }

    const auto text = extractJsonString(response.body, "response");
    if (!text.has_value()) {
        setLastError("Generate response did not include a response field.");
        return false;
    }

    output = trim(*text);
    m_lastError.clear();
    return true;
}

bool OllamaClient::chat(const std::string &modelName, const std::vector<ChatMessage> &messages, std::string &output)
{
    std::ostringstream body;
    body << "{\"model\":\"" << jsonEscape(modelName) << "\",\"messages\":[";
    for (std::size_t index = 0; index < messages.size(); ++index) {
        if (index > 0) {
            body << ',';
        }
        body << "{\"role\":\"" << jsonEscape(messages[index].role)
             << "\",\"content\":\"" << jsonEscape(messages[index].content) << "\"}";
    }
    body << "],\"stream\":false}";

    HttpResponse response;
    if (!request("POST", "/api/chat", body.str(), response)) {
        return false;
    }

    if (response.statusCode != 200) {
        setLastError("Chat request returned HTTP " + std::to_string(response.statusCode) + ": " + response.body);
        return false;
    }

    const auto content = extractJsonString(response.body, "content");
    if (!content.has_value()) {
        setLastError("Chat response did not include assistant content.");
        return false;
    }

    output = trim(*content);
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
