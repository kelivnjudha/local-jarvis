#pragma once

#include "AiTypes.h"

#include <string>
#include <vector>

namespace local_jarvis::ai {

class PromptBuilder {
public:
    [[nodiscard]] static std::string localAssistantSystemPrompt();
    [[nodiscard]] static std::string healthCheckPrompt();
    [[nodiscard]] static std::vector<ChatMessage> buildStudyNoteMessages(const std::string &transcript);
};

} // namespace local_jarvis::ai
