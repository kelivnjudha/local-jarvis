#pragma once

#include <string>

namespace local_jarvis::privacy {

struct CaptureStatus {
    bool microphoneEnabled = false;
    bool systemAudioEnabled = false;
    bool screenCaptureEnabled = false;
};

class PrivacyManager {
public:
    PrivacyManager() = default;

    void setMicrophoneEnabled(bool enabled);
    void setSystemAudioEnabled(bool enabled);
    void setScreenCaptureEnabled(bool enabled);
    void disableAllCapture();

    [[nodiscard]] CaptureStatus captureStatus() const;
    [[nodiscard]] std::string captureStatusText() const;

private:
    CaptureStatus m_status {};
};

} // namespace local_jarvis::privacy
