#include "asr/AsrEngineFactory.h"
#include "asr/AsrTypes.h"
#include "asr/AsrWorker.h"
#include "asr/AudioChunkBuffer.h"
#include "asr/StubAsrEngine.h"
#include "storage/Storage.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

std::vector<float> samples(std::size_t count, float value = 0.2F)
{
    return std::vector<float>(count, value);
}

} // namespace

int main()
{
    using local_jarvis::asr::AsrInputChunk;
    using local_jarvis::asr::AsrWorker;
    using local_jarvis::asr::AudioChunkBuffer;
    using local_jarvis::asr::StubAsrEngine;
    using local_jarvis::storage::Storage;
    using local_jarvis::storage::TranscriptSegmentInput;

#if LOCAL_JARVIS_ENABLE_WHISPER
    std::cerr << "This test verifies the default stub ASR path and expects LOCAL_JARVIS_ENABLE_WHISPER=OFF.\n";
    return EXIT_SUCCESS;
#endif

    AudioChunkBuffer buffer;
    buffer.configure(1000, 200, 2500);
    auto emitted = buffer.appendPcm("session-a", 0, 1000, 1, samples(1000));
    if (!expect(emitted.size() == 1, "AudioChunkBuffer should emit after enough samples.")) {
        return EXIT_FAILURE;
    }
    if (!expect(emitted.front().startMs == 0 && emitted.front().endMs == 1000, "First chunk timing should match buffered audio.")) {
        return EXIT_FAILURE;
    }
    if (!expect(buffer.bufferedSampleCount() == 200, "AudioChunkBuffer should retain configured overlap.")) {
        return EXIT_FAILURE;
    }

    emitted = buffer.appendPcm("session-a", 1000, 1000, 1, samples(800, 0.4F));
    if (!expect(emitted.size() == 1, "AudioChunkBuffer should use overlap for the next chunk.")) {
        return EXIT_FAILURE;
    }
    if (!expect(emitted.front().startMs == 800 && emitted.front().endMs == 1800, "Overlap chunk timing should start at the overlap boundary.")) {
        return EXIT_FAILURE;
    }

    const std::vector<float> stereo {
        1.0F, -1.0F,
        0.5F, 0.5F,
        0.25F, 0.75F
    };
    buffer.reset("session-stereo");
    buffer.configure(3, 0, 10);
    emitted = buffer.appendPcm("session-stereo", 0, 1000, 2, stereo);
    if (!expect(emitted.size() == 1 && emitted.front().channels == 1 && emitted.front().samples.size() == 3,
            "AudioChunkBuffer should mix interleaved stereo to mono chunks.")) {
        return EXIT_FAILURE;
    }

    buffer.reset("session-silence");
    buffer.configure(1000, 100, 1500);
    emitted = buffer.appendPcm("session-silence", 0, 1000, 1, std::span<const float> {});
    if (!expect(emitted.empty(), "Empty input should not emit or crash.")) {
        return EXIT_FAILURE;
    }
    emitted = buffer.appendPcm("session-silence", 0, 1000, 1, samples(5000, 0.0F));
    if (!expect(buffer.bufferedSampleCount() <= 1500, "AudioChunkBuffer should not grow unbounded.")) {
        return EXIT_FAILURE;
    }

    StubAsrEngine engine;
    if (!expect(engine.initialize(""), "Stub ASR engine should initialize without a model path.")) {
        return EXIT_FAILURE;
    }
    const auto firstResult = engine.transcribeChunk(AsrInputChunk {
        .chunkId = 1,
        .sessionId = "session-a",
        .startMs = 0,
        .endMs = 1000,
        .samples = samples(1000),
        .isFinalChunk = true
    });
    if (!expect(firstResult.ok && firstResult.text == "Stub transcript chunk 1", "Stub ASR should produce deterministic chunk text.")) {
        return EXIT_FAILURE;
    }
    const auto secondResult = engine.transcribeChunk(AsrInputChunk {
        .chunkId = 2,
        .sessionId = "session-a",
        .startMs = 1000,
        .endMs = 2000,
        .samples = samples(1000),
        .isFinalChunk = true
    });
    if (!expect(secondResult.ok && secondResult.text == "Stub transcript chunk 2", "Stub ASR should advance deterministic chunk text.")) {
        return EXIT_FAILURE;
    }

    auto captionSegment = local_jarvis::asr::toCaptionSegment(firstResult.segment);
    if (!expect(captionSegment.originalText == firstResult.text && captionSegment.speaker == "Microphone",
            "ASR transcript segments should convert to caption segments.")) {
        return EXIT_FAILURE;
    }

    std::mutex mutex;
    std::condition_variable condition;
    bool workerSegmentObserved = false;
    local_jarvis::asr::AsrTranscriptSegment workerSegment;
    AsrWorker worker(std::make_unique<StubAsrEngine>());
    worker.setSegmentCallback([&](const local_jarvis::asr::AsrTranscriptSegment &segment) {
        {
            std::lock_guard lock(mutex);
            workerSegmentObserved = true;
            workerSegment = segment;
        }
        condition.notify_one();
    });
    if (!expect(worker.start(), "AsrWorker should start with the stub backend.")) {
        return EXIT_FAILURE;
    }
    worker.enqueueChunk(AsrInputChunk {
        .chunkId = 7,
        .sessionId = "session-worker",
        .startMs = 200,
        .endMs = 1200,
        .samples = samples(1000),
        .isFinalChunk = true
    });
    {
        std::unique_lock lock(mutex);
        condition.wait_for(lock, std::chrono::seconds(2), [&]() {
            return workerSegmentObserved;
        });
    }
    worker.stop();
    if (!expect(workerSegmentObserved && workerSegment.text == "Stub transcript chunk 7",
            "AsrWorker should process chunks with the stub engine off the caller thread.")) {
        return EXIT_FAILURE;
    }

    const auto dbPath = std::filesystem::temp_directory_path() / "local-jarvis-asr-test.sqlite";
    std::filesystem::remove(dbPath);
    Storage storage;
    if (!expect(storage.initialize(dbPath), "ASR storage test database should initialize.")) {
        return EXIT_FAILURE;
    }
    const auto sessionId = storage.createSession("asr-test");
    if (!expect(sessionId.has_value(), "ASR storage test should create a session.")) {
        return EXIT_FAILURE;
    }
    const auto transcriptId = storage.addTranscriptSegment(TranscriptSegmentInput {
        .sessionId = *sessionId,
        .startMs = workerSegment.startMs,
        .endMs = workerSegment.endMs,
        .speaker = workerSegment.speaker,
        .text = workerSegment.text,
        .source = "microphone_asr_stub"
    });
    if (!expect(transcriptId.has_value() && storage.countTranscriptSegmentsForSession(*sessionId) == 1,
            "Final ASR transcript segments should store in transcript_segments.")) {
        return EXIT_FAILURE;
    }
    storage.close();
    std::filesystem::remove(dbPath);

    auto defaultEngine = local_jarvis::asr::createDefaultAsrEngine();
    if (!expect(defaultEngine->engineName() == "Stub", "LOCAL_JARVIS_ENABLE_WHISPER=OFF should build the Stub backend.")) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
