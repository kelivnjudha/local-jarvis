#pragma once

#include "AudioDeviceTypes.h"
#include "privacy/PrivacyManager.h"

#include <memory>
#include <string>
#include <vector>

namespace local_jarvis::audio {

class SystemAudioCapture {
public:
    virtual ~SystemAudioCapture() = default;

    [[nodiscard]] virtual std::vector<AudioOutputDevice> listOutputDevices() = 0;
    virtual bool selectOutputDevice(const std::string &deviceId) = 0;
    [[nodiscard]] virtual std::string selectedOutputDeviceId() const = 0;
    virtual bool startSystemAudioCapture() = 0;
    virtual void stopSystemAudioCapture() = 0;
    [[nodiscard]] virtual bool isSystemAudioActive() const = 0;
    [[nodiscard]] virtual double currentOutputLevel() const = 0;
    [[nodiscard]] virtual SystemAudioDiagnostics diagnostics() const = 0;
    [[nodiscard]] virtual std::string lastError() const = 0;
};

std::unique_ptr<SystemAudioCapture> createPlatformSystemAudioCapture(
    const privacy::PrivacyManager &privacyManager);

} // namespace local_jarvis::audio
