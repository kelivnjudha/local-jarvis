#include "AsrEngineFactory.h"

#if LOCAL_JARVIS_ENABLE_WHISPER
#include "WhisperAsrEngine.h"
#endif
#include "StubAsrEngine.h"

namespace local_jarvis::asr {

std::unique_ptr<AsrEngine> createDefaultAsrEngine()
{
    return std::make_unique<StubAsrEngine>();
}

std::unique_ptr<AsrEngine> createAsrEngine(AsrBackend backend)
{
    switch (backend) {
    case AsrBackend::Stub:
        return std::make_unique<StubAsrEngine>();
    case AsrBackend::Whisper:
#if LOCAL_JARVIS_ENABLE_WHISPER
        return std::make_unique<WhisperAsrEngine>();
#else
        return nullptr;
#endif
    }
    return std::make_unique<StubAsrEngine>();
}

bool whisperBackendBuildEnabled()
{
#if LOCAL_JARVIS_ENABLE_WHISPER
    return true;
#else
    return false;
#endif
}

} // namespace local_jarvis::asr
