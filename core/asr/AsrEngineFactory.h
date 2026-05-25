#pragma once

#include "AsrEngine.h"

#include <memory>

namespace local_jarvis::asr {

std::unique_ptr<AsrEngine> createDefaultAsrEngine();

} // namespace local_jarvis::asr
