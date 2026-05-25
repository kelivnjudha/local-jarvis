#include "ai/ModelClient.h"
#include "processing/MeetingNoteProcessor.h"
#include "processing/StudyNoteProcessor.h"
#include "storage/Storage.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

class FakeModelClient final : public local_jarvis::ai::ModelClient {
public:
    bool generate(const std::string &, const std::string &prompt, std::string &output) override
    {
        prompts.push_back(prompt);
        if (failGeneration) {
            m_lastError = failureMessage;
            return false;
        }

        output = nextOutput;
        m_lastError.clear();
        return true;
    }

    bool chat(const std::string &modelName, const std::vector<local_jarvis::ai::ChatMessage> &, std::string &output) override
    {
        return generate(modelName, "chat", output);
    }

    [[nodiscard]] const std::string &lastError() const override
    {
        return m_lastError;
    }

    std::string nextOutput;
    bool failGeneration = false;
    std::string failureMessage = "fake model failure";
    std::vector<std::string> prompts;

private:
    std::string m_lastError;
};

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

std::optional<std::string> createSessionWithContext(local_jarvis::storage::Storage &storage)
{
    const auto sessionId = storage.createSession("processing-test");
    if (!sessionId.has_value()) {
        return std::nullopt;
    }

    storage.addTranscriptSegment(local_jarvis::storage::TranscriptSegmentInput {
        .sessionId = *sessionId,
        .startMs = 0,
        .endMs = 1000,
        .speaker = std::string("Speaker"),
        .text = "Local Jarvis processing test transcript.",
        .source = "test"
    });
    storage.addScreenOcrSegment(local_jarvis::storage::ScreenOcrSegmentInput {
        .sessionId = *sessionId,
        .timestampMs = 500,
        .windowTitle = std::string("Test Window"),
        .text = "Processing test OCR context"
    });

    return sessionId;
}

bool testStudyChunk(local_jarvis::storage::Storage &storage)
{
    const auto sessionId = createSessionWithContext(storage);
    if (!expect(sessionId.has_value(), "Failed to create study test session.")) {
        return false;
    }

    FakeModelClient model;
    model.nextOutput = R"json({
        "title": "Study Alpha",
        "summary": "A compact study summary.",
        "key_points": ["Point one"],
        "review_questions": ["What is point one?"],
        "flashcards": [{"question":"Question","answer":"Answer","topic":"Topic"}]
    })json";

    local_jarvis::processing::StudyNoteProcessor processor(storage, model);
    const auto result = processor.processChunk(*sessionId, "fake-model");
    if (!expect(result.ok, "Study chunk processing should succeed.")) {
        return false;
    }

    const auto notes = storage.listLatestProcessedNotes(*sessionId, 5);
    const auto flashcards = storage.listFlashcards(*sessionId, 5);
    return expect(!notes.empty() && notes.front().type == "study_chunk", "Study note was not stored.")
        && expect(notes.front().jsonBody.has_value(), "Study note JSON body should be stored.")
        && expect(!flashcards.empty() && flashcards.front().question == "Question", "Study flashcard was not stored.");
}

bool testMeetingChunk(local_jarvis::storage::Storage &storage)
{
    const auto sessionId = createSessionWithContext(storage);
    if (!expect(sessionId.has_value(), "Failed to create meeting test session.")) {
        return false;
    }

    FakeModelClient model;
    model.nextOutput = R"json({
        "title": "Meeting Alpha",
        "summary": "A compact meeting summary.",
        "decisions": ["Ship hardening first"],
        "open_questions": ["What remains?"],
        "action_items": [{"text":"Review build docs","status":"open","due_at":null}]
    })json";

    local_jarvis::processing::MeetingNoteProcessor processor(storage, model);
    const auto result = processor.processChunk(*sessionId, "fake-model");
    if (!expect(result.ok, "Meeting chunk processing should succeed.")) {
        return false;
    }

    const auto notes = storage.listLatestProcessedNotes(*sessionId, 5);
    const auto actionItems = storage.listActionItems(*sessionId, 5);
    return expect(!notes.empty() && notes.front().type == "meeting_chunk", "Meeting note was not stored.")
        && expect(notes.front().jsonBody.has_value(), "Meeting note JSON body should be stored.")
        && expect(!actionItems.empty() && actionItems.front().text == "Review build docs", "Meeting action item was not stored.");
}

bool testFinalSummary(local_jarvis::storage::Storage &storage)
{
    const auto sessionId = createSessionWithContext(storage);
    if (!expect(sessionId.has_value(), "Failed to create final summary test session.")) {
        return false;
    }

    FakeModelClient model;
    model.nextOutput = R"json({
        "title": "Final Alpha",
        "summary": "A final summary.",
        "key_points": ["Final point"],
        "review_questions": [],
        "flashcards": []
    })json";

    local_jarvis::processing::StudyNoteProcessor processor(storage, model);
    const auto result = processor.processFinalSummary(*sessionId, "fake-model");
    if (!expect(result.ok, "Final summary processing should succeed.")) {
        return false;
    }

    const auto notes = storage.listLatestProcessedNotes(*sessionId, 5);
    return expect(!notes.empty() && notes.front().type == "final_summary", "Final summary note was not stored.");
}

bool testInvalidJson(local_jarvis::storage::Storage &storage)
{
    const auto sessionId = createSessionWithContext(storage);
    if (!expect(sessionId.has_value(), "Failed to create invalid JSON test session.")) {
        return false;
    }

    FakeModelClient model;
    model.nextOutput = "this is not json";

    local_jarvis::processing::StudyNoteProcessor processor(storage, model);
    const auto result = processor.processChunk(*sessionId, "fake-model");
    if (!expect(result.ok, "Invalid JSON output should be stored as raw output.")) {
        return false;
    }

    const auto notes = storage.listLatestProcessedNotes(*sessionId, 5);
    return expect(!notes.empty(), "Raw invalid JSON note was not stored.")
        && expect(notes.front().body == "this is not json", "Raw invalid JSON body was not preserved.")
        && expect(!notes.front().jsonBody.has_value(), "Invalid JSON should not store json_body.");
}

bool testModelFailure(local_jarvis::storage::Storage &storage)
{
    const auto sessionId = createSessionWithContext(storage);
    if (!expect(sessionId.has_value(), "Failed to create model failure test session.")) {
        return false;
    }

    FakeModelClient model;
    model.failGeneration = true;

    local_jarvis::processing::StudyNoteProcessor processor(storage, model);
    const auto result = processor.processChunk(*sessionId, "fake-model");
    const auto notes = storage.listLatestProcessedNotes(*sessionId, 5);
    return expect(!result.ok, "Model failure should return an error result.")
        && expect(notes.empty(), "Model failure should not create processed notes.");
}

} // namespace

int main()
{
    const auto dbPath = std::filesystem::temp_directory_path() / "local-jarvis-processing-test.sqlite";
    std::filesystem::remove(dbPath);

    local_jarvis::storage::Storage storage;
    if (!storage.initialize(dbPath)) {
        std::cerr << "Failed to initialize storage: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    const bool ok = testStudyChunk(storage)
        && testMeetingChunk(storage)
        && testFinalSummary(storage)
        && testInvalidJson(storage)
        && testModelFailure(storage);

    storage.close();
    std::filesystem::remove(dbPath);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
