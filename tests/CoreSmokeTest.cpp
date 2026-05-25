#include "audio/DummyAudioCapture.h"
#include "ai/AiTypes.h"
#include "ai/ModelManager.h"
#include "ai/OllamaClient.h"
#include "ai/PromptBuilder.h"
#include "asr/AsrEngineFactory.h"
#include "privacy/PrivacyManager.h"
#include "processing/JsonRepair.h"
#include "session/SessionManager.h"
#include "setup/SetupManager.h"
#include "setup/SystemCheck.h"
#include "storage/Storage.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>

int main()
{
    using local_jarvis::privacy::PrivacyManager;
    using local_jarvis::audio::DummyAudioCapture;
    using local_jarvis::asr::PcmAudioBuffer;
    using local_jarvis::session::SessionManager;
    using local_jarvis::session::SessionState;
    using local_jarvis::storage::ActionItemInput;
    using local_jarvis::storage::FlashcardInput;
    using local_jarvis::storage::ProcessedNoteInput;
    using local_jarvis::storage::PrivacyEventInput;
    using local_jarvis::storage::ScreenOcrSegmentInput;
    using local_jarvis::storage::Storage;
    using local_jarvis::storage::TranscriptSegmentInput;

    PrivacyManager privacy;
    auto asrEngine = local_jarvis::asr::createDefaultAsrEngine();
#if LOCAL_JARVIS_ENABLE_WHISPER
    if (asrEngine->engineName() != "whisper.cpp stub") {
        std::cerr << "Whisper build flag should select WhisperAsrEngine.\n";
        return EXIT_FAILURE;
    }
#else
    if (asrEngine->engineName() != "stub") {
        std::cerr << "Default ASR engine should be stub.\n";
        return EXIT_FAILURE;
    }
#endif

    if (!asrEngine->initialize("")) {
#if LOCAL_JARVIS_ENABLE_WHISPER
        // Whisper stub requires an explicit model path before it reports initialized.
#else
        std::cerr << "Stub ASR engine should initialize without a model path.\n";
        return EXIT_FAILURE;
#endif
    }
    const auto asrResult = asrEngine->transcribePcm(PcmAudioBuffer {});
    if (asrResult.ok || !asrResult.text.empty()) {
        std::cerr << "Prepared ASR stubs must not produce real transcripts.\n";
        return EXIT_FAILURE;
    }
    asrEngine->shutdown();

    const auto captureStatus = privacy.captureStatus();
    if (captureStatus.microphoneEnabled || captureStatus.systemAudioEnabled || captureStatus.screenCaptureEnabled) {
        std::cerr << "Capture must default to disabled.\n";
        return EXIT_FAILURE;
    }

    const auto defaultPath = Storage::defaultDatabasePath();
    if (defaultPath.filename() != "local_jarvis.db" || defaultPath.parent_path().filename() != "data") {
        std::cerr << "Default storage path should end with data/local_jarvis.db.\n";
        return EXIT_FAILURE;
    }

    const auto dbPath = std::filesystem::temp_directory_path() / "local-jarvis-core-smoke.sqlite";
    std::filesystem::remove(dbPath);

    Storage storage;
    if (!storage.initialize(dbPath)) {
        std::cerr << "Failed to initialize storage: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }
    if (storage.getSetting("schema_version").value_or("") != "1") {
        std::cerr << "Schema version was not recorded.\n";
        return EXIT_FAILURE;
    }

    if (!storage.setSetting("ai.current_model", local_jarvis::ai::kFallbackGemmaModel)) {
        std::cerr << "Failed to write setting: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }
    if (storage.getSetting("ai.current_model").value_or("") != local_jarvis::ai::kFallbackGemmaModel) {
        std::cerr << "Failed to read setting.\n";
        return EXIT_FAILURE;
    }
    if (!storage.addModelEvent("test_event", std::string(local_jarvis::ai::kFallbackGemmaModel), std::string("Smoke test model event")).has_value()) {
        std::cerr << "Failed to insert model event: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    local_jarvis::setup::SystemCheck systemCheck;
    local_jarvis::ai::OllamaClient offlineOllama("127.0.0.1", 1, 100);
    local_jarvis::ai::ModelManager modelManager(offlineOllama, systemCheck);
    const auto recommendedModel = modelManager.detectRecommendedModel();
    if (recommendedModel != local_jarvis::ai::kDefaultGemmaModel
        && recommendedModel != local_jarvis::ai::kFallbackGemmaModel) {
        std::cerr << "Recommended model should be one of the Gemma defaults.\n";
        return EXIT_FAILURE;
    }

    local_jarvis::setup::SetupManager setupManager(storage, modelManager, offlineOllama);
    const auto setupStatus = setupManager.firstRunCheck();
    if (!setupStatus.databaseReady || setupStatus.ollamaRunning || setupStatus.modelReady) {
        std::cerr << "Offline first-run setup status should have DB ready and local AI unavailable.\n";
        return EXIT_FAILURE;
    }

    if (local_jarvis::ai::PromptBuilder::healthCheckPrompt() != "Reply only with: LOCAL_JARVIS_READY") {
        std::cerr << "Health check prompt changed unexpectedly.\n";
        return EXIT_FAILURE;
    }

    const auto repairedJson = local_jarvis::processing::JsonRepair::normalizeJsonObject(
        "```json\n{\"summary\":\"ok\",\"flashcards\":[{\"question\":\"Q\",\"answer\":\"A\"}]}\n```");
    if (!repairedJson.has_value()
        || local_jarvis::processing::JsonRepair::extractString(*repairedJson, "summary").value_or("") != "ok"
        || local_jarvis::processing::JsonRepair::extractObjectArray(*repairedJson, "flashcards").empty()) {
        std::cerr << "JSON repair helper failed.\n";
        return EXIT_FAILURE;
    }

    const auto storedSessionId = storage.createSession("study", std::string("Storage smoke session"));
    if (!storedSessionId.has_value()) {
        std::cerr << "Failed to create storage session: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    const auto transcriptId = storage.addTranscriptSegment(TranscriptSegmentInput {
        .sessionId = *storedSessionId,
        .startMs = 0,
        .endMs = 1200,
        .speaker = std::string("Speaker 1"),
        .text = "This is a local alpha transcript placeholder.",
        .source = "manual-test"
    });
    if (!transcriptId.has_value()) {
        std::cerr << "Failed to insert transcript segment: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    if (!storage.addScreenOcrSegment(ScreenOcrSegmentInput {
            .sessionId = *storedSessionId,
            .timestampMs = 1500,
            .windowTitle = std::string("Local Jarvis"),
            .text = "Visible local OCR placeholder"
        }).has_value()) {
        std::cerr << "Failed to insert screen OCR segment: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    const auto noteId = storage.addProcessedNote(ProcessedNoteInput {
        .sessionId = *storedSessionId,
        .type = "summary",
        .title = std::string("Smoke test note"),
        .body = "Storage can persist processed notes with banana search text.",
        .jsonBody = std::string("{\"kind\":\"summary\"}")
    });
    if (!noteId.has_value()) {
        std::cerr << "Failed to insert processed note: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    if (!storage.addActionItem(ActionItemInput {
            .sessionId = *storedSessionId,
            .text = "Review Local Jarvis storage",
            .status = "open"
        }).has_value()) {
        std::cerr << "Failed to insert action item: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    if (!storage.addFlashcard(FlashcardInput {
            .sessionId = *storedSessionId,
            .question = "Where does Local Jarvis store data?",
            .answer = "In a local SQLite database.",
            .topic = std::string("storage")
        }).has_value()) {
        std::cerr << "Failed to insert flashcard: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    if (!storage.addPrivacyEvent(PrivacyEventInput {
            .sessionId = storedSessionId,
            .eventType = "test_privacy_event",
            .details = "Smoke test privacy event"
        }).has_value()) {
        std::cerr << "Failed to insert privacy event: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    if (!storage.endSession(*storedSessionId)) {
        std::cerr << "Failed to end storage session: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    const auto recentSessions = storage.listRecentSessions(5);
    if (recentSessions.empty() || recentSessions.front().id.empty()) {
        std::cerr << "Failed to list recent sessions: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }
    if (recentSessions.front().summaryStatus.empty()) {
        std::cerr << "Recent session should include summary status.\n";
        return EXIT_FAILURE;
    }

    if (storage.listRecentTranscriptSegments(*storedSessionId, 5).empty()
        || storage.listRecentScreenOcrSegments(*storedSessionId, 5).empty()
        || storage.listLatestProcessedNotes(*storedSessionId, 5).empty()
        || storage.listActionItems(*storedSessionId, 5).empty()
        || storage.listFlashcards(*storedSessionId, 5).empty()) {
        std::cerr << "Storage list helpers returned empty results.\n";
        return EXIT_FAILURE;
    }

    if (storage.searchTranscripts("alpha", 5).empty()) {
        std::cerr << "Transcript search returned no results.\n";
        return EXIT_FAILURE;
    }

    if (storage.searchProcessedNotes("banana", 5).empty()) {
        std::cerr << "Processed note search returned no results.\n";
        return EXIT_FAILURE;
    }

    DummyAudioCapture disabledAudio(privacy, std::chrono::milliseconds(25));
    SessionManager disabledCaptureSession(storage, disabledAudio);
    const auto disabledCaptureStarted = disabledCaptureSession.startSession("test-disabled-capture");
    if (!disabledCaptureStarted.ok) {
        std::cerr << "Failed to start disabled-capture session: " << disabledCaptureStarted.message << '\n';
        return EXIT_FAILURE;
    }
    if (disabledAudio.isMicrophoneActive() || disabledAudio.isSystemAudioActive()) {
        std::cerr << "Session must start with capture disabled when privacy permissions are disabled.\n";
        return EXIT_FAILURE;
    }
    disabledCaptureSession.stopSession();

    privacy.setMicrophoneEnabled(true);
    if (!privacy.captureStatus().microphoneEnabled) {
        std::cerr << "Enabling microphone did not update privacy status.\n";
        return EXIT_FAILURE;
    }

    std::mutex transcriptMutex;
    std::condition_variable transcriptCondition;
    bool transcriptObserved = false;

    DummyAudioCapture dummyAudio(privacy, std::chrono::milliseconds(25));
    SessionManager dummySession(storage, dummyAudio);
    dummySession.setTranscriptCallback([&](const local_jarvis::audio::TranscriptEvent &) {
        {
            std::lock_guard lock(transcriptMutex);
            transcriptObserved = true;
        }
        transcriptCondition.notify_one();
    });

    const auto dummyStarted = dummySession.startSession("test-dummy-audio");
    if (!dummyStarted.ok || !dummyStarted.session.has_value()) {
        std::cerr << "Failed to start dummy audio session: " << dummyStarted.message << '\n';
        return EXIT_FAILURE;
    }
    if (!dummyAudio.isMicrophoneActive()) {
        std::cerr << "Microphone dummy capture did not start after explicit permission.\n";
        return EXIT_FAILURE;
    }

    {
        std::unique_lock lock(transcriptMutex);
        transcriptCondition.wait_for(lock, std::chrono::seconds(2), [&]() {
            return transcriptObserved;
        });
    }

    dummySession.stopSession();

    const int storedTranscriptCount = storage.countTranscriptSegmentsForSession(dummyStarted.session->id);
    if (!transcriptObserved || storedTranscriptCount < 1) {
        std::cerr << "Dummy transcript segment was not stored.\n";
        return EXIT_FAILURE;
    }

    SessionManager sessions(storage);
    const auto started = sessions.startSession();
    if (!started.ok || sessions.state() != SessionState::Active || !sessions.currentSessionId().has_value()) {
        std::cerr << "Failed to start session: " << started.message << '\n';
        return EXIT_FAILURE;
    }

    const auto stopped = sessions.stopSession();
    if (!stopped.ok || sessions.state() != SessionState::Stopped || sessions.currentSessionId().has_value()) {
        std::cerr << "Failed to stop session: " << stopped.message << '\n';
        return EXIT_FAILURE;
    }

    storage.close();
    std::filesystem::remove(dbPath);
    return EXIT_SUCCESS;
}
