#pragma once

#include "AudioCapture.h"
#include "AudioDeviceTypes.h"
#include "privacy/PrivacyManager.h"

#include <memory>
#include <string>
#include <vector>

namespace local_jarvis::audio {

class MicrophoneCapture : public AudioCapture {
public:
    ~MicrophoneCapture() override = default;

    [[nodiscard]] virtual std::vector<AudioInputDevice> listInputDevices() = 0;
    virtual bool selectInputDevice(const std::string &deviceId) = 0;
    [[nodiscard]] virtual std::string selectedInputDeviceId() const = 0;
    [[nodiscard]] virtual double currentInputLevel() const = 0;
    [[nodiscard]] virtual std::string lastError() const = 0;
};

std::unique_ptr<MicrophoneCapture> createPlatformMicrophoneCapture(
    const privacy::PrivacyManager &privacyManager);

} // namespace local_jarvis::audio
