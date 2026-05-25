#include "StubAsrEngine.h"

namespace local_jarvis::asr {

bool StubAsrEngine::initialize(const std::string &)
{
    m_initialized = true;
    return true;
}

AsrResult StubAsrEngine::transcribePcm(const PcmAudioBuffer &)
{
    return {
        .ok = false,
        .text = {},
        .message = "ASR stub is active; no speech-to-text was performed."
    };
}

void StubAsrEngine::shutdown()
{
    m_initialized = false;
}

std::string StubAsrEngine::engineName() const
{
    return "stub";
}

bool StubAsrEngine::isInitialized() const
{
    return m_initialized;
}

} // namespace local_jarvis::asr
