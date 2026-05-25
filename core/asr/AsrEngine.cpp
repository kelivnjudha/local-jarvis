#include "AsrEngine.h"

namespace local_jarvis::asr {

void AsrEngine::configure(const AsrEngineConfig &)
{
}

AsrResult AsrEngine::transcribePcm(const PcmAudioBuffer &audioBuffer)
{
    AsrInputChunk chunk {
        .chunkId = 1,
        .sampleRate = audioBuffer.sampleRateHz,
        .channels = audioBuffer.channelCount,
        .samples = audioBuffer.samples,
        .isFinalChunk = true
    };
    return transcribeChunk(chunk);
}

bool AsrEngine::isReady() const
{
    return isInitialized();
}

std::string AsrEngine::lastError() const
{
    return {};
}

} // namespace local_jarvis::asr
