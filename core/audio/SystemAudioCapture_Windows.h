#pragma once

#include "AudioLevelMeter.h"
#include "SystemAudioCapture.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace local_jarvis::audio {

class WindowsSystemAudioCapture final : public SystemAudioCapture {
public:
    explicit WindowsSystemAudioCapture(const privacy::PrivacyManager &privacyManager);
    ~WindowsSystemAudioCapture() override;

    WindowsSystemAudioCapture(const WindowsSystemAudioCapture &) = delete;
    WindowsSystemAudioCapture &operator=(const WindowsSystemAudioCapture &) = delete;

    [[nodiscard]] std::vector<AudioOutputDevice> listOutputDevices() override;
    bool selectOutputDevice(const std::string &deviceId) override;
    [[nodiscard]] std::string selectedOutputDeviceId() const override;
    bool startSystemAudioCapture() override;
    void stopSystemAudioCapture() override;
    void setPcmAudioCallback(PcmAudioCallback callback) override;
    [[nodiscard]] bool isSystemAudioActive() const override;
    [[nodiscard]] double currentOutputLevel() const override;
    [[nodiscard]] SystemAudioDiagnostics diagnostics() const override;
    [[nodiscard]] std::string lastError() const override;

private:
    void captureLoop();
    void completeStart(bool ok, const std::string &error);
    [[nodiscard]] bool stopRequested() const;
    void setLastError(const std::string &error);

    const privacy::PrivacyManager &m_privacyManager;
    mutable std::mutex m_mutex;
    std::condition_variable m_startCondition;
    std::thread m_worker;
    AudioLevelMeter m_levelMeter;
    SystemAudioDiagnostics m_diagnostics;
    std::string m_selectedDeviceId;
    std::string m_lastError;
    PcmAudioCallback m_pcmAudioCallback;
    bool m_systemAudioActive = false;
    bool m_stopRequested = false;
    bool m_startCompleted = false;
};

} // namespace local_jarvis::audio
