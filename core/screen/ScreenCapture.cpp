#include "ScreenCapture.h"

#include "ScreenCapturePlatform.h"

namespace local_jarvis::screen {

ScreenCapture::ScreenCapture(const privacy::PrivacyManager &privacyManager)
    : m_privacyManager(privacyManager)
{
    m_status.message = "Screen capture is stopped.";
}

ScreenCaptureStatus ScreenCapture::start(const ScreenCaptureConfig &)
{
    if (!m_privacyManager.captureStatus().screenCaptureEnabled) {
        m_status = {
            .state = ScreenCaptureState::PermissionRequired,
            .message = "Screen capture requires explicit user permission before it can start."
        };
        return m_status;
    }

    m_status = {
        .state = ScreenCaptureState::Stopped,
        .message = std::string("Screen capture is not implemented yet; selected platform stub: ")
            + platform::backendName()
    };
    return m_status;
}

void ScreenCapture::stop()
{
    m_status = {
        .state = ScreenCaptureState::Stopped,
        .message = "Screen capture is stopped."
    };
}

ScreenCaptureStatus ScreenCapture::status() const
{
    return m_status;
}

} // namespace local_jarvis::screen
