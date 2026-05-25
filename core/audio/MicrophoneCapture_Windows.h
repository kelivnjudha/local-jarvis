#pragma once

#include "AudioLevelMeter.h"
#include "MicrophoneCapture.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace local_jarvis::audio {

class WindowsMicrophoneCapture final : public MicrophoneCapture {
public:
    explicit WindowsMicrophoneCapture(const privacy::PrivacyManager &privacyManager);
    ~WindowsMicrophoneCapture() override;

    WindowsMicrophoneCapture(const WindowsMicrophoneCapture &) = delete;
    WindowsMicrophoneCapture &operator=(const WindowsMicrophoneCapture &) = delete;

    [[nodiscard]] std::vector<AudioInputDevice> listInputDevices() override;
    bool selectInputDevice(const std::string &deviceId) override;
    [[nodiscard]] std::string selectedInputDeviceId() const override;
    bool startMicrophoneCapture() override;
    void stopMicrophoneCapture() override;
    bool startSystemAudioCapture() override;
    void stopSystemAudioCapture() override;
    [[nodiscard]] bool isMicrophoneActive() const override;
    [[nodiscard]] bool isSystemAudioActive() const override;
    void setTranscriptCallback(TranscriptCallback callback) override;
    [[nodiscard]] double currentInputLevel() const override;
    [[nodiscard]] MicrophoneDiagnostics diagnostics() const override;
    [[nodiscard]] std::string lastError() const override;
    void setPcmAudioCallback(PcmAudioCallback callback) override;

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
    MicrophoneDiagnostics m_diagnostics;
    std::string m_selectedDeviceId;
    std::string m_lastError;
    TranscriptCallback m_transcriptCallback;
    PcmAudioCallback m_pcmAudioCallback;
    bool m_microphoneActive = false;
    bool m_systemAudioActive = false;
    bool m_stopRequested = false;
    bool m_startCompleted = false;
};

} // namespace local_jarvis::audio
