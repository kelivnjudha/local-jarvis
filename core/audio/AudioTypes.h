#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace local_jarvis::audio {

enum class AudioSource {
    Microphone,
    SystemAudio
};

enum class CaptureState {
    Stopped,
    PermissionRequired,
    Running
};

struct AudioCaptureConfig {
    AudioSource source = AudioSource::Microphone;
};

struct AudioCaptureStatus {
    CaptureState state = CaptureState::Stopped;
    std::string message;
};

struct TranscriptEvent {
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    std::optional<std::string> speaker;
    std::string text;
    std::string source;
};

} // namespace local_jarvis::audio
