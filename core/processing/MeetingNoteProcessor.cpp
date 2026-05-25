#include "MeetingNoteProcessor.h"

#include "JsonRepair.h"
#include "ai/PromptBuilder.h"

#include <sstream>

namespace local_jarvis::processing {
namespace {

std::string joinBullets(const std::vector<std::string> &values)
{
    std::ostringstream stream;
    for (const auto &value : values) {
        if (!value.empty()) {
            stream << "- " << value << '\n';
        }
    }
    return stream.str();
}

} // namespace

MeetingNoteProcessor::MeetingNoteProcessor(storage::Storage &storage, ai::ModelClient &modelClient)
    : m_storage(storage)
    , m_modelClient(modelClient)
{
}

ProcessingResult MeetingNoteProcessor::processChunk(const std::string &sessionId, const std::string &modelName)
{
    const std::string prompt = ai::PromptBuilder::buildMeetingChunkPrompt(
        sessionId,
        buildTranscriptContext(sessionId),
        buildScreenOcrContext(sessionId));

    std::string output;
    if (!m_modelClient.generate(modelName, prompt, output)) {
        return { false, "Model meeting chunk generation failed: " + m_modelClient.lastError(), {} };
    }

    return persistMeetingOutput(sessionId, modelName, output);
}

std::string MeetingNoteProcessor::buildTranscriptContext(const std::string &sessionId) const
{
    std::ostringstream stream;
    for (const auto &segment : m_storage.listRecentTranscriptSegments(sessionId, 60)) {
        stream << '[' << segment.startMs << '-' << segment.endMs << " ms] ";
        if (segment.speaker.has_value()) {
            stream << *segment.speaker << ": ";
        }
        stream << segment.text << '\n';
    }
    return stream.str();
}

std::string MeetingNoteProcessor::buildScreenOcrContext(const std::string &sessionId) const
{
    std::ostringstream stream;
    for (const auto &segment : m_storage.listRecentScreenOcrSegments(sessionId, 30)) {
        stream << '[' << segment.timestampMs << " ms] ";
        if (segment.windowTitle.has_value()) {
            stream << *segment.windowTitle << ": ";
        }
        stream << segment.text << '\n';
    }
    return stream.str();
}

ProcessingResult MeetingNoteProcessor::persistMeetingOutput(
    const std::string &sessionId,
    const std::string &modelName,
    const std::string &rawOutput)
{
    const auto json = JsonRepair::normalizeJsonObject(rawOutput);
    if (!json.has_value()) {
        m_storage.addProcessedNote(storage::ProcessedNoteInput {
            .sessionId = sessionId,
            .type = "meeting_chunk",
            .title = std::string("Raw local model output"),
            .body = rawOutput,
            .jsonBody = std::nullopt
        });
        m_storage.addModelEvent("json_parse_failed", modelName, "Meeting processor stored raw output for session " + sessionId);
        return { true, "Stored raw meeting output because JSON parsing failed.", rawOutput };
    }

    const std::string title = JsonRepair::extractString(*json, "title").value_or("Meeting notes");
    const std::string summary = JsonRepair::extractString(*json, "summary").value_or(*json);
    const std::string decisions = joinBullets(JsonRepair::extractStringArray(*json, "decisions"));
    const std::string openQuestions = joinBullets(JsonRepair::extractStringArray(*json, "open_questions"));

    std::ostringstream body;
    body << summary;
    if (!decisions.empty()) {
        body << "\n\nDecisions:\n" << decisions;
    }
    if (!openQuestions.empty()) {
        body << "\nOpen questions:\n" << openQuestions;
    }

    m_storage.addProcessedNote(storage::ProcessedNoteInput {
        .sessionId = sessionId,
        .type = "meeting_chunk",
        .title = title,
        .body = body.str(),
        .jsonBody = *json
    });

    for (const auto &object : JsonRepair::extractObjectArray(*json, "action_items")) {
        const auto text = JsonRepair::extractString(object, "text");
        if (!text.has_value()) {
            continue;
        }

        m_storage.addActionItem(storage::ActionItemInput {
            .sessionId = sessionId,
            .text = *text,
            .status = JsonRepair::extractString(object, "status").value_or("open"),
            .dueAt = JsonRepair::extractString(object, "due_at")
        });
    }

    return { true, "Stored meeting processing output.", rawOutput };
}

} // namespace local_jarvis::processing
