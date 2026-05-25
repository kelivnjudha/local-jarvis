#include "PrivacyManager.h"

#include <sstream>

namespace local_jarvis::privacy {

void PrivacyManager::setMicrophoneEnabled(bool enabled)
{
    m_status.microphoneEnabled = enabled;
}

void PrivacyManager::setSystemAudioEnabled(bool enabled)
{
    m_status.systemAudioEnabled = enabled;
}

void PrivacyManager::setScreenCaptureEnabled(bool enabled)
{
    m_status.screenCaptureEnabled = enabled;
}

void PrivacyManager::disableAllCapture()
{
    m_status = {};
}

CaptureStatus PrivacyManager::captureStatus() const
{
    return m_status;
}

std::string PrivacyManager::captureStatusText() const
{
    std::ostringstream stream;
    stream << "Microphone: " << (m_status.microphoneEnabled ? "enabled" : "disabled")
           << ", System audio: " << (m_status.systemAudioEnabled ? "enabled" : "disabled")
           << ", Screen: " << (m_status.screenCaptureEnabled ? "enabled" : "disabled");
    return stream.str();
}

} // namespace local_jarvis::privacy
