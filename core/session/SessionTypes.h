#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace local_jarvis::session {

enum class SessionState {
    Stopped,
    Active
};

struct SessionInfo {
    std::string id;
    std::chrono::system_clock::time_point startedAt;
    std::optional<std::chrono::system_clock::time_point> endedAt;
};

struct SessionResult {
    bool ok = false;
    std::string message;
    std::optional<SessionInfo> session;
};

} // namespace local_jarvis::session
