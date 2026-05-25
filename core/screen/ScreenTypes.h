#pragma once

#include <string>

namespace local_jarvis::screen {

enum class ScreenCaptureState {
    Stopped,
    PermissionRequired,
    Running
};

struct ScreenCaptureConfig {
    int displayIndex = 0;
};

struct ScreenCaptureStatus {
    ScreenCaptureState state = ScreenCaptureState::Stopped;
    std::string message;
};

} // namespace local_jarvis::screen
