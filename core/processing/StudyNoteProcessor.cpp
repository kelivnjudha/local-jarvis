#include "StudyNoteProcessor.h"

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

StudyNoteProcessor::StudyNoteProcessor(storage::Storage &storage, ai::ModelClient &modelClient)
    : m_storage(storage)
    , m_modelClient(modelClient)
{
}

ProcessingResult StudyNoteProcessor::processChunk(const std::string &sessionId, const std::string &modelName)
{
    const std::string prompt = ai::PromptBuilder::buildStudyChunkPrompt(
        sessionId,
        buildTranscriptContext(sessionId),
        buildScreenOcrContext(sessionId));

    std::string output;
    if (!m_modelClient.generate(modelName, prompt, output)) {
        return { false, "Model study chunk generation failed: " + m_modelClient.lastError(), {} };
    }

    return persistStudyOutput(sessionId, modelName, "study_chunk", output);
}

ProcessingResult StudyNoteProcessor::processFinalSummary(const std::string &sessionId, const std::string &modelName)
{
    const std::string prompt = ai::PromptBuilder::buildFinalSummaryPrompt(
        sessionId,
        buildTranscriptContext(sessionId),
        buildScreenOcrContext(sessionId));

    std::string output;
    if (!m_modelClient.generate(modelName, prompt, output)) {
        return { false, "Model final summary generation failed: " + m_modelClient.lastError(), {} };
    }

    return persistStudyOutput(sessionId, modelName, "final_summary", output);
}

std::string StudyNoteProcessor::buildTranscriptContext(const std::string &sessionId) const
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

std::string StudyNoteProcessor::buildScreenOcrContext(const std::string &sessionId) const
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

ProcessingResult StudyNoteProcessor::persistStudyOutput(
    const std::string &sessionId,
    const std::string &modelName,
    const std::string &noteType,
    const std::string &rawOutput)
{
    const auto json = JsonRepair::normalizeJsonObject(rawOutput);
    if (!json.has_value()) {
        m_storage.addProcessedNote(storage::ProcessedNoteInput {
            .sessionId = sessionId,
            .type = noteType,
            .title = std::string("Raw local model output"),
            .body = rawOutput,
            .jsonBody = std::nullopt
        });
        m_storage.addModelEvent("json_parse_failed", modelName, "Study processor stored raw output for session " + sessionId);
        return { true, "Stored raw study output because JSON parsing failed.", rawOutput };
    }

    const std::string title = JsonRepair::extractString(*json, "title").value_or("Study notes");
    const std::string summary = JsonRepair::extractString(*json, "summary").value_or(*json);
    const std::string keyPoints = joinBullets(JsonRepair::extractStringArray(*json, "key_points"));
    const std::string reviewQuestions = joinBullets(JsonRepair::extractStringArray(*json, "review_questions"));

    std::ostringstream body;
    body << summary;
    if (!keyPoints.empty()) {
        body << "\n\nKey points:\n" << keyPoints;
    }
    if (!reviewQuestions.empty()) {
        body << "\nReview questions:\n" << reviewQuestions;
    }

    m_storage.addProcessedNote(storage::ProcessedNoteInput {
        .sessionId = sessionId,
        .type = noteType,
        .title = title,
        .body = body.str(),
        .jsonBody = *json
    });

    for (const auto &object : JsonRepair::extractObjectArray(*json, "flashcards")) {
        const auto question = JsonRepair::extractString(object, "question");
        const auto answer = JsonRepair::extractString(object, "answer");
        if (!question.has_value() || !answer.has_value()) {
            continue;
        }

        m_storage.addFlashcard(storage::FlashcardInput {
            .sessionId = sessionId,
            .question = *question,
            .answer = *answer,
            .topic = JsonRepair::extractString(object, "topic")
        });
    }

    return { true, "Stored study processing output.", rawOutput };
}

} // namespace local_jarvis::processing
