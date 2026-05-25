#pragma once

#include "AiTypes.h"

#include <string>
#include <vector>

namespace local_jarvis::ai {

class ModelClient {
public:
    virtual ~ModelClient() = default;

    virtual bool generate(const std::string &modelName, const std::string &prompt, std::string &output) = 0;
    virtual bool chat(const std::string &modelName, const std::vector<ChatMessage> &messages, std::string &output) = 0;

    [[nodiscard]] virtual const std::string &lastError() const = 0;
};

} // namespace local_jarvis::ai
