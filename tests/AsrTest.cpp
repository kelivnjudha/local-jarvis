#include "asr/AsrEngineFactory.h"
#include "asr/AsrTypes.h"
#include "asr/AsrWorker.h"
#include "asr/AudioChunkBuffer.h"
#include "asr/StubAsrEngine.h"
#include "asr/WhisperAudioConversion.h"
#include "storage/Storage.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
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

class CountingWhisperLikeEngine final : public local_jarvis::asr::AsrEngine {
public:
    explicit CountingWhisperLikeEngine(std::shared_ptr<int> callCount)
        : m_callCount(std::move(callCount))
    {
    }

    bool initialize(const std::string &) override
    {
        m_initialized = true;
        return true;
    }

    local_jarvis::asr::AsrResult transcribeChunk(const local_jarvis::asr::AsrInputChunk &chunk) override
    {
        ++(*m_callCount);
        const std::string text = "fake speech " + std::to_string(chunk.chunkId);
        return local_jarvis::asr::AsrResult {
            .ok = true,
            .segment = local_jarvis::asr::AsrTranscriptSegment {
                .id = "fake-" + std::to_string(chunk.chunkId),
                .sessionId = chunk.sessionId,
                .startMs = chunk.startMs,
                .endMs = chunk.endMs,
                .speaker = "Microphone",
                .text = text,
                .detectedLanguage = "en",
                .confidence = 0.75,
                .isFinal = true
            },
            .text = text
        };
    }

    void shutdown() override
    {
        m_initialized = false;
    }

    std::string engineName() const override
    {
        return "Whisper";
    }

    bool isInitialized() const override
    {
        return m_initialized;
    }

private:
    std::shared_ptr<int> m_callCount;
    bool m_initialized = false;
};

class BlankWhisperLikeEngine final : public local_jarvis::asr::AsrEngine {
public:
    explicit BlankWhisperLikeEngine(std::string text)
        : m_text(std::move(text))
    {
    }

    bool initialize(const std::string &) override
    {
        m_initialized = true;
        return true;
    }

    local_jarvis::asr::AsrResult transcribeChunk(const local_jarvis::asr::AsrInputChunk &chunk) override
    {
        return local_jarvis::asr::AsrResult {
            .ok = true,
            .segment = local_jarvis::asr::AsrTranscriptSegment {
                .id = "blank-" + std::to_string(chunk.chunkId),
                .sessionId = chunk.sessionId,
                .startMs = chunk.startMs,
                .endMs = chunk.endMs,
                .speaker = "Microphone",
                .text = m_text,
                .detectedLanguage = "en",
                .confidence = 0.0,
                .isFinal = true
            },
            .text = m_text
        };
    }

    void shutdown() override
    {
        m_initialized = false;
    }

    std::string engineName() const override
    {
        return "Whisper";
    }

    bool isInitialized() const override
    {
        return m_initialized;
    }

private:
    std::string m_text;
    bool m_initialized = false;
};

bool waitForStats(local_jarvis::asr::AsrWorker &worker, const std::function<bool(const local_jarvis::asr::AsrWorkerStats &)> &predicate)
{
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (predicate(worker.stats())) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    return false;
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
    buffer.reset("session-system");
    buffer.configure(1000, 0, 2000);
    emitted = buffer.appendPcm(
        "session-system",
        0,
        1000,
        1,
        samples(1000),
        false,
        local_jarvis::asr::AsrAudioSource::SystemAudio);
    if (!expect(emitted.size() == 1 && emitted.front().audioSource == local_jarvis::asr::AsrAudioSource::SystemAudio,
            "AudioChunkBuffer should preserve system audio source metadata.")) {
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
    const auto systemResult = engine.transcribeChunk(AsrInputChunk {
        .chunkId = 3,
        .sessionId = "session-a",
        .startMs = 2000,
        .endMs = 3000,
        .samples = samples(1000),
        .isFinalChunk = true,
        .audioSource = local_jarvis::asr::AsrAudioSource::SystemAudio
    });
    if (!expect(systemResult.ok
            && systemResult.segment.audioSource == local_jarvis::asr::AsrAudioSource::SystemAudio
            && systemResult.segment.speaker == "System"
            && local_jarvis::asr::transcriptSourceFor(local_jarvis::asr::AsrBackend::Stub, systemResult.segment.audioSource) == "system_audio_asr_stub",
            "System audio ASR should map to the system_audio_asr_stub transcript source.")) {
        return EXIT_FAILURE;
    }

    auto captionSegment = local_jarvis::asr::toCaptionSegment(firstResult.segment);
    if (!expect(captionSegment.originalText == firstResult.text && captionSegment.speaker == "Mic",
            "ASR transcript segments should convert to caption segments.")) {
        return EXIT_FAILURE;
    }
    const auto systemCaptionSegment = local_jarvis::asr::toCaptionSegment(systemResult.segment);
    if (!expect(systemCaptionSegment.speaker == "System",
            "System audio transcript segments should convert to System caption labels.")) {
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
    const auto workerStatsBeforeStop = worker.stats();
    worker.stop();
    if (!expect(workerSegmentObserved && workerSegment.text == "Stub transcript chunk 7",
            "AsrWorker should process chunks with the stub engine off the caller thread.")) {
        return EXIT_FAILURE;
    }
    if (!expect(workerStatsBeforeStop.lastChunkId == 7
            && workerStatsBeforeStop.lastChunkDurationMs == 1000
            && workerStatsBeforeStop.lastChunkSampleRate == 16000
            && workerStatsBeforeStop.lastWhisperSampleCount == 1000,
            "AsrWorker should expose last chunk timing and Whisper-prepared sample count.")) {
        return EXIT_FAILURE;
    }
    if (!expect(workerStatsBeforeStop.lastChunkRms > 0.19
            && workerStatsBeforeStop.lastChunkPeak > 0.19
            && !workerStatsBeforeStop.lastChunkTreatedAsSilent
            && workerStatsBeforeStop.lastTranscriptText == "Stub transcript chunk 7"
            && workerStatsBeforeStop.microphone.chunksProcessed == 1,
            "AsrWorker should expose RMS, peak, silence, and last text diagnostics.")) {
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
    const auto systemTranscriptId = storage.addTranscriptSegment(TranscriptSegmentInput {
        .sessionId = *sessionId,
        .startMs = systemResult.segment.startMs,
        .endMs = systemResult.segment.endMs,
        .speaker = systemResult.segment.speaker,
        .text = systemResult.segment.text,
        .source = "system_audio_asr_stub"
    });
    if (!expect(systemTranscriptId.has_value() && storage.countTranscriptSegmentsForSession(*sessionId) == 2,
            "System audio ASR transcript segments should store with system_audio_asr_stub source.")) {
        return EXIT_FAILURE;
    }
    if (!expect(storage.setSetting("asr.backend", "whisper")
            && storage.setSetting("asr.whisper.model_path", "C:/models/ggml-base.bin")
            && storage.getSetting("asr.backend").value_or("") == "whisper",
            "ASR backend/model settings should save and load through Storage.")) {
        return EXIT_FAILURE;
    }
    storage.close();
    std::filesystem::remove(dbPath);

    auto defaultEngine = local_jarvis::asr::createDefaultAsrEngine();
    if (!expect(defaultEngine->engineName() == "Stub", "LOCAL_JARVIS_ENABLE_WHISPER=OFF should build the Stub backend.")) {
        return EXIT_FAILURE;
    }

    auto explicitStub = local_jarvis::asr::createAsrEngine(local_jarvis::asr::AsrBackend::Stub);
    if (!expect(explicitStub && explicitStub->engineName() == "Stub", "Explicit Stub backend should always be available.")) {
        return EXIT_FAILURE;
    }
#if !LOCAL_JARVIS_ENABLE_WHISPER
    auto disabledWhisper = local_jarvis::asr::createAsrEngine(local_jarvis::asr::AsrBackend::Whisper);
    if (!expect(!disabledWhisper, "Whisper backend should be unavailable when LOCAL_JARVIS_ENABLE_WHISPER=OFF.")) {
        return EXIT_FAILURE;
    }
#endif

    const std::vector<float> ramp { -1.0F, -0.5F, 0.0F, 0.5F, 1.0F };
    const auto sameRate = local_jarvis::asr::resampleToWhisperRate(ramp, local_jarvis::asr::kWhisperSampleRate);
    if (!expect(sameRate == ramp, "Whisper conversion should preserve same-rate normalized samples.")) {
        return EXIT_FAILURE;
    }
    const auto downsampled = local_jarvis::asr::resampleToWhisperRate(samples(48000, 0.25F), 48000);
    if (!expect(downsampled.size() >= 15900 && downsampled.size() <= 16100, "Whisper conversion should resample 48 kHz audio to about 16 kHz.")) {
        return EXIT_FAILURE;
    }
    if (!expect(local_jarvis::asr::isProbablySilent(samples(16000, 0.0F)), "Whisper conversion should classify zero buffers as silent.")) {
        return EXIT_FAILURE;
    }
    if (!expect(!local_jarvis::asr::isProbablySilent(samples(16000, 0.1F)), "Whisper conversion should keep audible synthetic samples.")) {
        return EXIT_FAILURE;
    }

    {
        auto callCount = std::make_shared<int>(0);
        AsrWorker skipWorker(std::make_unique<CountingWhisperLikeEngine>(callCount));
        if (!expect(skipWorker.start(), "Whisper-like worker should start for silence skip test.")) {
            return EXIT_FAILURE;
        }
        skipWorker.enqueueChunk(AsrInputChunk {
            .chunkId = 101,
            .sessionId = "session-skip",
            .startMs = 0,
            .endMs = 3000,
            .samples = samples(48000, 0.0F),
            .isFinalChunk = true
        });
        if (!expect(waitForStats(skipWorker, [](const auto &stats) { return stats.chunksSkippedSilence == 1; }),
                "Silent Whisper chunks should be skipped before reaching the engine.")) {
            return EXIT_FAILURE;
        }
        skipWorker.stop();
        if (!expect(*callCount == 0, "Skipped silence should not call the Whisper engine.")) {
            return EXIT_FAILURE;
        }
    }

    {
        auto callCount = std::make_shared<int>(0);
        AsrWorker quietWorker(std::make_unique<CountingWhisperLikeEngine>(callCount));
        if (!expect(quietWorker.start(), "Whisper-like worker should start for too-quiet skip test.")) {
            return EXIT_FAILURE;
        }
        quietWorker.enqueueChunk(AsrInputChunk {
            .chunkId = 102,
            .sessionId = "session-quiet",
            .startMs = 0,
            .endMs = 3000,
            .samples = samples(48000, 0.002F),
            .isFinalChunk = true
        });
        if (!expect(waitForStats(quietWorker, [](const auto &stats) { return stats.chunksSkippedTooQuiet == 1; }),
                "Too-quiet Whisper chunks should be skipped by default.")) {
            return EXIT_FAILURE;
        }
        quietWorker.stop();
        if (!expect(*callCount == 0, "Skipped too-quiet chunks should not call the Whisper engine.")) {
            return EXIT_FAILURE;
        }
    }

    {
        auto callCount = std::make_shared<int>(0);
        std::mutex speechMutex;
        std::condition_variable speechCondition;
        bool speechObserved = false;
        AsrWorker speechWorker(std::make_unique<CountingWhisperLikeEngine>(callCount));
        speechWorker.setSegmentCallback([&](const local_jarvis::asr::AsrTranscriptSegment &) {
            {
                std::lock_guard lock(speechMutex);
                speechObserved = true;
            }
            speechCondition.notify_one();
        });
        if (!expect(speechWorker.start(), "Whisper-like worker should start for speech processing test.")) {
            return EXIT_FAILURE;
        }
        speechWorker.enqueueChunk(AsrInputChunk {
            .chunkId = 103,
            .sessionId = "session-speech",
            .startMs = 0,
            .endMs = 3000,
            .samples = samples(48000, 0.12F),
            .isFinalChunk = true
        });
        {
            std::unique_lock lock(speechMutex);
            speechCondition.wait_for(lock, std::chrono::seconds(2), [&]() { return speechObserved; });
        }
        speechWorker.stop();
        if (!expect(speechObserved && *callCount == 1, "Likely speech chunks should reach the Whisper engine.")) {
            return EXIT_FAILURE;
        }
    }

    {
        std::mutex blankMutex;
        std::condition_variable blankCondition;
        bool blankObserved = false;
        AsrWorker blankWorker(std::make_unique<BlankWhisperLikeEngine>("[BLANK_AUDIO]"));
        blankWorker.setSegmentCallback([&](const local_jarvis::asr::AsrTranscriptSegment &) {
            {
                std::lock_guard lock(blankMutex);
                blankObserved = true;
            }
            blankCondition.notify_one();
        });
        if (!expect(blankWorker.start(), "Whisper-like worker should start for blank output test.")) {
            return EXIT_FAILURE;
        }
        blankWorker.enqueueChunk(AsrInputChunk {
            .chunkId = 104,
            .sessionId = "session-blank",
            .startMs = 0,
            .endMs = 3000,
            .samples = samples(48000, 0.12F),
            .isFinalChunk = true,
            .audioSource = local_jarvis::asr::AsrAudioSource::SystemAudio
        });
        if (!expect(waitForStats(blankWorker, [](const auto &stats) {
                return stats.blankOutputs == 1 && stats.systemAudio.blankOutputs == 1;
            }),
                "[BLANK_AUDIO] should be counted and suppressed for system audio.")) {
            return EXIT_FAILURE;
        }
        {
            std::unique_lock lock(blankMutex);
            blankCondition.wait_for(lock, std::chrono::milliseconds(100), [&]() { return blankObserved; });
        }
        blankWorker.stop();
        if (!expect(!blankObserved, "[BLANK_AUDIO] should not emit a transcript segment.")) {
            return EXIT_FAILURE;
        }
    }

    {
        AsrWorker emptyWorker(std::make_unique<BlankWhisperLikeEngine>("   "));
        if (!expect(emptyWorker.start(), "Whisper-like worker should start for empty output test.")) {
            return EXIT_FAILURE;
        }
        emptyWorker.enqueueChunk(AsrInputChunk {
            .chunkId = 105,
            .sessionId = "session-empty",
            .startMs = 0,
            .endMs = 3000,
            .samples = samples(48000, 0.12F),
            .isFinalChunk = true
        });
        if (!expect(waitForStats(emptyWorker, [](const auto &stats) { return stats.blankOutputs == 1; }),
                "Whitespace ASR output should be counted and suppressed.")) {
            return EXIT_FAILURE;
        }
        emptyWorker.stop();
    }

    return EXIT_SUCCESS;
}
