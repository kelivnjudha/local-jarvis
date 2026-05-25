#pragma once

#include <string>

namespace local_jarvis::llm {

struct LlmResult {
    bool ok = false;
    std::string text;
    std::string message;
};

class LlmEngine {
public:
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] LlmResult processPlaceholder(const std::string &input) const;
};

} // namespace local_jarvis::llm
