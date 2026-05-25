#pragma once

#include "AiTypes.h"
#include "ModelClient.h"

#include <string>
#include <vector>

namespace local_jarvis::ai {

class OllamaClient final : public ModelClient {
public:
    explicit OllamaClient(
        std::string host = kOllamaHost,
        int port = kOllamaPort,
        int timeoutMs = 3000);

    [[nodiscard]] bool isOllamaRunning();
    [[nodiscard]] bool isModelInstalled(const std::string &modelName);
    [[nodiscard]] std::vector<std::string> listLocalModels();

    bool generate(const std::string &modelName, const std::string &prompt, std::string &output) override;
    bool chat(const std::string &modelName, const std::vector<ChatMessage> &messages, std::string &output) override;

    [[nodiscard]] const std::string &lastError() const override;

private:
    struct HttpResponse {
        int statusCode = 0;
        std::string body;
    };

    bool request(const std::string &method, const std::string &path, const std::string &body, HttpResponse &response);
    void setLastError(std::string message);

    std::string m_host;
    int m_port = kOllamaPort;
    int m_timeoutMs = 3000;
    std::string m_lastError;
};

} // namespace local_jarvis::ai
