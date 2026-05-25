#pragma once

#include "ScreenTypes.h"
#include "privacy/PrivacyManager.h"

namespace local_jarvis::screen {

class ScreenCapture {
public:
    explicit ScreenCapture(const privacy::PrivacyManager &privacyManager);

    ScreenCaptureStatus start(const ScreenCaptureConfig &config);
    void stop();

    [[nodiscard]] ScreenCaptureStatus status() const;

private:
    const privacy::PrivacyManager &m_privacyManager;
    ScreenCaptureStatus m_status {};
};

} // namespace local_jarvis::screen
