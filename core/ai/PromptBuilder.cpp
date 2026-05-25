#include "PromptBuilder.h"

namespace local_jarvis::ai {

std::string PromptBuilder::localAssistantSystemPrompt()
{
    return "You are Local Jarvis, a local-only study and meeting assistant. "
           "Do not request cloud services, hidden capture, evasion, or deceptive automation.";
}

std::string PromptBuilder::healthCheckPrompt()
{
    return "Reply only with: LOCAL_JARVIS_READY";
}

std::vector<ChatMessage> PromptBuilder::buildStudyNoteMessages(const std::string &transcript)
{
    return {
        { "system", localAssistantSystemPrompt() },
        { "user", "Create concise study notes from this local transcript:\n\n" + transcript }
    };
}

} // namespace local_jarvis::ai
