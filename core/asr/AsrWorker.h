#pragma once

#include "AsrEngine.h"
#include "AsrTypes.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace local_jarvis::asr {

struct AsrWorkerStats {
    AsrStatus status = AsrStatus::Disabled;
    std::uint64_t chunksQueued = 0;
    std::uint64_t chunksProcessed = 0;
    std::size_t pendingChunks = 0;
    std::uint64_t lastChunkId = 0;
    std::int64_t lastChunkDurationMs = 0;
    int lastChunkSampleRate = 0;
    int lastChunkChannels = 0;
    std::size_t lastChunkInputSamples = 0;
    std::size_t lastWhisperSampleCount = 0;
    double lastChunkRms = 0.0;
    double lastChunkPeak = 0.0;
    double lastChunkDbfs = -120.0;
    double lastChunkNonZeroRatio = 0.0;
    bool lastChunkTreatedAsSilent = false;
    std::string lastTranscriptText;
    std::string lastError;
};

class AsrWorker {
public:
    using SegmentCallback = std::function<void(const AsrTranscriptSegment &)>;
    using StatusCallback = std::function<void(AsrStatus, const std::string &)>;

    explicit AsrWorker(std::unique_ptr<AsrEngine> engine);
    ~AsrWorker();

    AsrWorker(const AsrWorker &) = delete;
    AsrWorker &operator=(const AsrWorker &) = delete;

    [[nodiscard]] bool start(const std::string &modelPath = {});
    void stop();
    void setEngine(std::unique_ptr<AsrEngine> engine);
    void setConfig(const AsrEngineConfig &config);
    void enqueueChunk(const AsrInputChunk &chunk);
    void setListening(bool listening);

    void setSegmentCallback(SegmentCallback callback);
    void setStatusCallback(StatusCallback callback);

    [[nodiscard]] AsrWorkerStats stats() const;
    [[nodiscard]] std::string engineName() const;
    [[nodiscard]] bool isRunning() const;

private:
    void workerLoop();
    void publishStatus(AsrStatus status, const std::string &message);

    std::unique_ptr<AsrEngine> m_engine;
    AsrEngineConfig m_config;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<AsrInputChunk> m_queue;
    std::thread m_worker;
    SegmentCallback m_segmentCallback;
    StatusCallback m_statusCallback;
    AsrStatus m_status = AsrStatus::Disabled;
    std::string m_lastError;
    std::uint64_t m_chunksQueued = 0;
    std::uint64_t m_chunksProcessed = 0;
    AsrWorkerStats m_lastDiagnostics;
    bool m_stopRequested = false;
    bool m_running = false;
};

} // namespace local_jarvis::asr
