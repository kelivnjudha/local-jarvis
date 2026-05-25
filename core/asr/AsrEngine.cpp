#include "AsrEngine.h"

namespace local_jarvis::asr {

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

} // namespace local_jarvis::asr
