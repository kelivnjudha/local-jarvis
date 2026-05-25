#include "WhisperAsrEngine.h"

namespace local_jarvis::asr {

bool WhisperAsrEngine::initialize(const std::string &modelPath)
{
    m_modelPath = modelPath;
    m_initialized = !m_modelPath.empty();
    return m_initialized;
}

AsrResult WhisperAsrEngine::transcribeChunk(const AsrInputChunk &chunk)
{
#if LOCAL_JARVIS_WHISPER_LINKED
    (void)chunk;
    return {
        .ok = false,
        .text = {},
        .message = "Whisper ASR integration hooks are linked, but transcription is not implemented in this scaffold."
    };
#else
    (void)chunk;
    return {
        .ok = false,
        .text = {},
        .message = "Whisper ASR was enabled, but the whisper.cpp target is not linked."
    };
#endif
}

void WhisperAsrEngine::shutdown()
{
    m_initialized = false;
    m_modelPath.clear();
}

std::string WhisperAsrEngine::engineName() const
{
    return "Whisper";
}

bool WhisperAsrEngine::isInitialized() const
{
    return m_initialized;
}

} // namespace local_jarvis::asr
