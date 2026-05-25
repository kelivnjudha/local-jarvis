#pragma once

#include "AsrEngine.h"

#include <memory>

namespace local_jarvis::asr {

std::unique_ptr<AsrEngine> createDefaultAsrEngine();
std::unique_ptr<AsrEngine> createAsrEngine(AsrBackend backend);
[[nodiscard]] bool whisperBackendBuildEnabled();

} // namespace local_jarvis::asr
