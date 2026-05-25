#include "LlmEngine.h"

namespace local_jarvis::llm {

bool LlmEngine::isAvailable() const
{
    return false;
}

LlmResult LlmEngine::processPlaceholder(const std::string &) const
{
    return {
        .ok = false,
        .text = {},
        .message = "Local LLM processing is not wired yet."
    };
}

} // namespace local_jarvis::llm
