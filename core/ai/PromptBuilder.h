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
    [[nodiscard]] static std::string buildStudyChunkPrompt(
        const std::string &sessionId,
        const std::string &transcriptContext,
        const std::string &screenOcrContext);
    [[nodiscard]] static std::string buildMeetingChunkPrompt(
        const std::string &sessionId,
        const std::string &transcriptContext,
        const std::string &screenOcrContext);
    [[nodiscard]] static std::string buildFinalSummaryPrompt(
        const std::string &sessionId,
        const std::string &transcriptContext,
        const std::string &screenOcrContext);
};

} // namespace local_jarvis::ai
