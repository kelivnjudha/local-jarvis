#include "AsrEngineFactory.h"

#if LOCAL_JARVIS_ENABLE_WHISPER
#include "WhisperAsrEngine.h"
#else
#include "StubAsrEngine.h"
#endif

namespace local_jarvis::asr {

std::unique_ptr<AsrEngine> createDefaultAsrEngine()
{
#if LOCAL_JARVIS_ENABLE_WHISPER
    return std::make_unique<WhisperAsrEngine>();
#else
    return std::make_unique<StubAsrEngine>();
#endif
}

} // namespace local_jarvis::asr
