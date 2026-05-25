#include "StubAsrEngine.h"

#include <string>

namespace local_jarvis::asr {

bool StubAsrEngine::initialize(const std::string &)
{
    m_initialized = true;
    return true;
}

AsrResult StubAsrEngine::transcribeChunk(const AsrInputChunk &chunk)
{
    if (!m_initialized) {
        return {
            .ok = false,
            .message = "ASR stub is not initialized."
        };
    }

    const std::uint64_t visibleChunkId = chunk.chunkId == 0 ? m_nextChunkNumber++ : chunk.chunkId;
    const std::string text = "Stub transcript chunk " + std::to_string(visibleChunkId);
    return {
        .ok = true,
        .segment = AsrTranscriptSegment {
            .id = "asr-stub-" + std::to_string(visibleChunkId),
            .sessionId = chunk.sessionId,
            .startMs = chunk.startMs,
            .endMs = chunk.endMs,
            .speaker = "Microphone",
            .text = text,
            .detectedLanguage = "en",
            .confidence = 1.0,
            .isFinal = true
        },
        .text = text,
        .message = "ASR stub produced a deterministic local transcript segment."
    };
}

void StubAsrEngine::shutdown()
{
    m_initialized = false;
    m_nextChunkNumber = 1;
}

std::string StubAsrEngine::engineName() const
{
    return "Stub";
}

bool StubAsrEngine::isInitialized() const
{
    return m_initialized;
}

} // namespace local_jarvis::asr
