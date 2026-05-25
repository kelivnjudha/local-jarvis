#include "audio/DummyAudioCapture.h"
#include "asr/AsrEngineFactory.h"
#include "privacy/PrivacyManager.h"
#include "session/SessionManager.h"
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
    using local_jarvis::storage::ProcessedNoteInput;
    using local_jarvis::storage::Storage;
    using local_jarvis::storage::TranscriptSegmentInput;

    PrivacyManager privacy;
    const auto asrEngine = local_jarvis::asr::createDefaultAsrEngine();
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

    const auto defaultPathRoot = std::filesystem::temp_directory_path() / "local-jarvis-default-path-smoke";
    std::filesystem::remove_all(defaultPathRoot);
    std::filesystem::create_directories(defaultPathRoot);

    const auto originalPath = std::filesystem::current_path();
    std::filesystem::current_path(defaultPathRoot);
    {
        Storage defaultStorage;
        if (!defaultStorage.open() || !defaultStorage.createSchema()) {
            std::cerr << "Failed to initialize default storage path: " << defaultStorage.lastError() << '\n';
            std::filesystem::current_path(originalPath);
            return EXIT_FAILURE;
        }

        defaultStorage.close();
    }
    if (!std::filesystem::exists(defaultPathRoot / "data" / "local_jarvis.db")) {
        std::cerr << "Default storage path did not create ./data/local_jarvis.db.\n";
        std::filesystem::current_path(originalPath);
        return EXIT_FAILURE;
    }
    std::filesystem::current_path(originalPath);
    std::filesystem::remove_all(defaultPathRoot);

    const auto dbPath = std::filesystem::temp_directory_path() / "local-jarvis-core-smoke.sqlite";
    std::filesystem::remove(dbPath);

    Storage storage;
    if (!storage.open(dbPath) || !storage.createSchema()) {
        std::cerr << "Failed to initialize storage: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    const auto storedSessionId = storage.createSession("study");
    if (!storedSessionId.has_value()) {
        std::cerr << "Failed to create storage session: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    const auto transcriptId = storage.addTranscriptSegment(TranscriptSegmentInput {
        .sessionId = *storedSessionId,
        .startMs = 0,
        .endMs = 1200,
        .speaker = std::string("Speaker 1"),
        .text = "This is a local transcript placeholder.",
        .source = "manual-test"
    });
    if (!transcriptId.has_value()) {
        std::cerr << "Failed to insert transcript segment: " << storage.lastError() << '\n';
        return EXIT_FAILURE;
    }

    const auto noteId = storage.addProcessedNote(ProcessedNoteInput {
        .sessionId = *storedSessionId,
        .type = "summary",
        .title = "Smoke test note",
        .body = "Storage can persist processed notes."
    });
    if (!noteId.has_value()) {
        std::cerr << "Failed to insert processed note: " << storage.lastError() << '\n';
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
