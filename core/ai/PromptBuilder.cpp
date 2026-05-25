#include "PromptBuilder.h"

#include <sstream>

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

std::string PromptBuilder::buildStudyChunkPrompt(
    const std::string &sessionId,
    const std::string &transcriptContext,
    const std::string &screenOcrContext)
{
    std::ostringstream prompt;
    prompt << localAssistantSystemPrompt() << "\n\n"
           << "Process this local-only study session chunk. Session ID: " << sessionId << "\n"
           << "Return strict JSON only. Do not wrap it in markdown. Do not include commentary.\n"
           << "Schema:\n"
           << "{\n"
           << "  \"title\": \"short title\",\n"
           << "  \"summary\": \"concise study explanation\",\n"
           << "  \"key_points\": [\"point\"],\n"
           << "  \"review_questions\": [\"question\"],\n"
           << "  \"flashcards\": [{\"question\":\"question\",\"answer\":\"answer\",\"topic\":\"topic or null\"}]\n"
           << "}\n\n"
           << "Transcript context:\n" << transcriptContext << "\n"
           << "Screen OCR context:\n" << screenOcrContext << "\n";
    return prompt.str();
}

std::string PromptBuilder::buildMeetingChunkPrompt(
    const std::string &sessionId,
    const std::string &transcriptContext,
    const std::string &screenOcrContext)
{
    std::ostringstream prompt;
    prompt << localAssistantSystemPrompt() << "\n\n"
           << "Process this local-only meeting session chunk. Session ID: " << sessionId << "\n"
           << "Return strict JSON only. Do not wrap it in markdown. Do not include commentary.\n"
           << "Schema:\n"
           << "{\n"
           << "  \"title\": \"short title\",\n"
           << "  \"summary\": \"concise meeting summary\",\n"
           << "  \"decisions\": [\"decision\"],\n"
           << "  \"open_questions\": [\"question\"],\n"
           << "  \"action_items\": [{\"text\":\"task\",\"status\":\"open\",\"due_at\":\"ISO date or null\"}]\n"
           << "}\n\n"
           << "Transcript context:\n" << transcriptContext << "\n"
           << "Screen OCR context:\n" << screenOcrContext << "\n";
    return prompt.str();
}

std::string PromptBuilder::buildFinalSummaryPrompt(
    const std::string &sessionId,
    const std::string &transcriptContext,
    const std::string &screenOcrContext)
{
    std::ostringstream prompt;
    prompt << localAssistantSystemPrompt() << "\n\n"
           << "Create a final local-only session summary. Session ID: " << sessionId << "\n"
           << "Return strict JSON only. Do not wrap it in markdown. Do not include commentary.\n"
           << "Schema:\n"
           << "{\n"
           << "  \"title\": \"session summary title\",\n"
           << "  \"summary\": \"complete summary\",\n"
           << "  \"key_points\": [\"important point\"],\n"
           << "  \"review_questions\": [\"question\"],\n"
           << "  \"flashcards\": [{\"question\":\"question\",\"answer\":\"answer\",\"topic\":\"topic or null\"}]\n"
           << "}\n\n"
           << "Transcript context:\n" << transcriptContext << "\n"
           << "Screen OCR context:\n" << screenOcrContext << "\n";
    return prompt.str();
}

} // namespace local_jarvis::ai
