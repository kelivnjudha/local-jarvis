#pragma once

#include "AsrTypes.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace local_jarvis::asr {

class AudioChunkBuffer {
public:
    void configure(std::int64_t chunkDurationMs = 3000, std::int64_t overlapDurationMs = 300, std::int64_t maxBufferedMs = 30000);
    void reset(const std::string &sessionId = {});

    [[nodiscard]] std::vector<AsrInputChunk> appendPcm(
        const std::string &sessionId,
        std::int64_t startMs,
        int sampleRate,
        int channels,
        std::span<const float> samples,
        bool isFinalChunk = false,
        AsrAudioSource audioSource = AsrAudioSource::Microphone);

    [[nodiscard]] std::vector<AsrInputChunk> flush(const std::string &sessionId);

    [[nodiscard]] std::size_t bufferedSampleCount() const;
    [[nodiscard]] std::uint64_t chunksEmitted() const;

private:
    [[nodiscard]] std::vector<AsrInputChunk> emitReadyChunksLocked(
        const std::string &sessionId,
        bool flushRemainder,
        AsrAudioSource audioSource);
    [[nodiscard]] std::int64_t framesToMs(std::size_t frameCount) const;
    void enforceBoundLocked();

    mutable std::mutex m_mutex;
    std::vector<float> m_samples;
    std::string m_sessionId;
    std::int64_t m_bufferStartMs = 0;
    std::int64_t m_chunkDurationMs = 3000;
    std::int64_t m_overlapDurationMs = 300;
    std::int64_t m_maxBufferedMs = 30000;
    int m_sampleRate = 0;
    std::uint64_t m_nextChunkId = 1;
    std::uint64_t m_chunksEmitted = 0;
};

} // namespace local_jarvis::asr
