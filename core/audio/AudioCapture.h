#pragma once

#include "AudioTypes.h"

#include <functional>

namespace local_jarvis::audio {

class AudioCapture {
public:
    using TranscriptCallback = std::function<void(const TranscriptEvent &)>;

    virtual ~AudioCapture() = default;

    virtual bool startMicrophoneCapture() = 0;
    virtual void stopMicrophoneCapture() = 0;
    virtual bool startSystemAudioCapture() = 0;
    virtual void stopSystemAudioCapture() = 0;
    [[nodiscard]] virtual bool isMicrophoneActive() const = 0;
    [[nodiscard]] virtual bool isSystemAudioActive() const = 0;
    virtual void setTranscriptCallback(TranscriptCallback callback) = 0;
};

} // namespace local_jarvis::audio
