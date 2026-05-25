#include "WhisperAsrEngine.h"

namespace local_jarvis::asr {

bool WhisperAsrEngine::initialize(const std::string &modelPath)
{
    m_modelPath = modelPath;
    m_initialized = !m_modelPath.empty();
    return m_initialized;
}

AsrResult WhisperAsrEngine::transcribePcm(const PcmAudioBuffer &)
{
    return {
        .ok = false,
        .text = {},
        .message = "Whisper ASR integration is prepared but not implemented yet."
    };
}

void WhisperAsrEngine::shutdown()
{
    m_initialized = false;
    m_modelPath.clear();
}

std::string WhisperAsrEngine::engineName() const
{
    return "whisper.cpp stub";
}

bool WhisperAsrEngine::isInitialized() const
{
    return m_initialized;
}

} // namespace local_jarvis::asr
