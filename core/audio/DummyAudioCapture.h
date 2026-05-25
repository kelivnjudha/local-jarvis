#pragma once

#include "MicrophoneCapture.h"
#include "privacy/PrivacyManager.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace local_jarvis::audio {

class DummyAudioCapture final : public MicrophoneCapture {
public:
    explicit DummyAudioCapture(
        const privacy::PrivacyManager &privacyManager,
        std::chrono::milliseconds interval = std::chrono::seconds(3));
    ~DummyAudioCapture() override;

    DummyAudioCapture(const DummyAudioCapture &) = delete;
    DummyAudioCapture &operator=(const DummyAudioCapture &) = delete;

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

private:
    void startWorkerIfNeeded();
    void stopWorkerIfIdle();
    void stopWorker();
    void workerLoop();
    void emitTranscript(bool microphoneActive, bool systemAudioActive);

    const privacy::PrivacyManager &m_privacyManager;
    std::chrono::milliseconds m_interval;

    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_worker;
    bool m_microphoneActive = false;
    bool m_systemAudioActive = false;
    bool m_stopRequested = false;
    std::int64_t m_nextStartMs = 0;
    std::uint64_t m_sequence = 0;
    std::string m_selectedDeviceId = "dummy-microphone";
    std::string m_lastError;
    MicrophoneDiagnostics m_diagnostics;
    TranscriptCallback m_transcriptCallback;
};

} // namespace local_jarvis::audio
