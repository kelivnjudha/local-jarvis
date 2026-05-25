#include "AudioChunkBuffer.h"

#include "audio/AudioLevelMeter.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace local_jarvis::asr {
namespace {

std::int64_t clampDuration(std::int64_t value, std::int64_t minimum, std::int64_t maximum)
{
    return std::clamp(value, minimum, maximum);
}

std::size_t durationToFrames(std::int64_t durationMs, int sampleRate)
{
    if (durationMs <= 0 || sampleRate <= 0) {
        return 0;
    }
    const double frames = (static_cast<double>(sampleRate) * static_cast<double>(durationMs)) / 1000.0;
    return static_cast<std::size_t>(std::max(1.0, std::round(frames)));
}

} // namespace

void AudioChunkBuffer::configure(std::int64_t chunkDurationMs, std::int64_t overlapDurationMs, std::int64_t maxBufferedMs)
{
    std::lock_guard lock(m_mutex);
    m_chunkDurationMs = clampDuration(chunkDurationMs, 1, 30000);
    m_overlapDurationMs = clampDuration(overlapDurationMs, 0, m_chunkDurationMs / 2);
    m_maxBufferedMs = clampDuration(maxBufferedMs, m_chunkDurationMs, 120000);
    enforceBoundLocked();
}

void AudioChunkBuffer::reset(const std::string &sessionId)
{
    std::lock_guard lock(m_mutex);
    m_samples.clear();
    m_sessionId = sessionId;
    m_bufferStartMs = 0;
    m_sampleRate = 0;
    m_nextChunkId = 1;
    m_chunksEmitted = 0;
}

std::vector<AsrInputChunk> AudioChunkBuffer::appendPcm(
    const std::string &sessionId,
    std::int64_t startMs,
    int sampleRate,
    int channels,
    std::span<const float> samples,
    bool isFinalChunk)
{
    if (sampleRate <= 0) {
        return {};
    }

    std::vector<float> monoSamples;
    if (!samples.empty()) {
        monoSamples = audio::AudioLevelMeter::mixInterleavedToMono(samples, channels <= 0 ? 1 : channels);
    }

    std::lock_guard lock(m_mutex);
    if (m_sessionId != sessionId || m_sampleRate != sampleRate) {
        m_samples.clear();
        m_sessionId = sessionId;
        m_sampleRate = sampleRate;
        m_bufferStartMs = startMs;
        m_nextChunkId = 1;
        m_chunksEmitted = 0;
    }

    if (m_samples.empty()) {
        m_bufferStartMs = startMs;
    }

    if (!monoSamples.empty()) {
        m_samples.insert(m_samples.end(), monoSamples.begin(), monoSamples.end());
    } else if (!isFinalChunk) {
        return {};
    }

    auto chunks = emitReadyChunksLocked(sessionId, isFinalChunk);
    enforceBoundLocked();
    return chunks;
}

std::vector<AsrInputChunk> AudioChunkBuffer::flush(const std::string &sessionId)
{
    std::lock_guard lock(m_mutex);
    return emitReadyChunksLocked(sessionId, true);
}

std::size_t AudioChunkBuffer::bufferedSampleCount() const
{
    std::lock_guard lock(m_mutex);
    return m_samples.size();
}

std::uint64_t AudioChunkBuffer::chunksEmitted() const
{
    std::lock_guard lock(m_mutex);
    return m_chunksEmitted;
}

std::vector<AsrInputChunk> AudioChunkBuffer::emitReadyChunksLocked(const std::string &sessionId, bool flushRemainder)
{
    std::vector<AsrInputChunk> chunks;
    if (m_sampleRate <= 0 || m_samples.empty()) {
        return chunks;
    }

    const std::size_t chunkFrames = durationToFrames(m_chunkDurationMs, m_sampleRate);
    const std::size_t overlapFrames = std::min(durationToFrames(m_overlapDurationMs, m_sampleRate), chunkFrames / 2);

    while (chunkFrames > 0 && m_samples.size() >= chunkFrames) {
        std::vector<float> chunkSamples(m_samples.begin(), m_samples.begin() + static_cast<std::ptrdiff_t>(chunkFrames));
        const std::int64_t chunkStartMs = m_bufferStartMs;
        const std::int64_t chunkEndMs = chunkStartMs + framesToMs(chunkFrames);
        chunks.push_back(AsrInputChunk {
            .chunkId = m_nextChunkId++,
            .sessionId = sessionId,
            .startMs = chunkStartMs,
            .endMs = chunkEndMs,
            .sampleRate = m_sampleRate,
            .channels = 1,
            .samples = std::move(chunkSamples),
            .isFinalChunk = false
        });
        ++m_chunksEmitted;

        const std::size_t framesToRemove = std::max<std::size_t>(1, chunkFrames - overlapFrames);
        m_samples.erase(m_samples.begin(), m_samples.begin() + static_cast<std::ptrdiff_t>(framesToRemove));
        m_bufferStartMs += framesToMs(framesToRemove);
    }

    if (flushRemainder && !m_samples.empty()) {
        const std::size_t remainingFrames = m_samples.size();
        const std::int64_t chunkStartMs = m_bufferStartMs;
        const std::int64_t chunkEndMs = chunkStartMs + framesToMs(remainingFrames);
        chunks.push_back(AsrInputChunk {
            .chunkId = m_nextChunkId++,
            .sessionId = sessionId,
            .startMs = chunkStartMs,
            .endMs = chunkEndMs,
            .sampleRate = m_sampleRate,
            .channels = 1,
            .samples = std::move(m_samples),
            .isFinalChunk = true
        });
        ++m_chunksEmitted;
        m_samples.clear();
        m_bufferStartMs = chunkEndMs;
    }

    return chunks;
}

std::int64_t AudioChunkBuffer::framesToMs(std::size_t frameCount) const
{
    if (m_sampleRate <= 0) {
        return 0;
    }
    return static_cast<std::int64_t>(
        std::llround((static_cast<double>(frameCount) * 1000.0) / static_cast<double>(m_sampleRate)));
}

void AudioChunkBuffer::enforceBoundLocked()
{
    if (m_sampleRate <= 0) {
        return;
    }

    const std::size_t maxFrames = durationToFrames(m_maxBufferedMs, m_sampleRate);
    if (maxFrames == 0 || m_samples.size() <= maxFrames) {
        return;
    }

    const std::size_t framesToDrop = m_samples.size() - maxFrames;
    m_samples.erase(m_samples.begin(), m_samples.begin() + static_cast<std::ptrdiff_t>(framesToDrop));
    m_bufferStartMs += framesToMs(framesToDrop);
}

} // namespace local_jarvis::asr
