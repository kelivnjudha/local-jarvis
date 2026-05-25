#include "MicrophoneCapture.h"

#if defined(LOCAL_JARVIS_PLATFORM_WINDOWS)
#include "MicrophoneCapture_Windows.h"
#endif

#include <mutex>
#include <utility>

namespace local_jarvis::audio {
namespace {

#if !defined(LOCAL_JARVIS_PLATFORM_WINDOWS)
class UnavailableMicrophoneCapture final : public MicrophoneCapture {
public:
    explicit UnavailableMicrophoneCapture(const privacy::PrivacyManager &privacyManager)
        : m_privacyManager(privacyManager)
    {
    }

    std::vector<AudioInputDevice> listInputDevices() override
    {
        return {};
    }

    bool selectInputDevice(const std::string &deviceId) override
    {
        std::lock_guard lock(m_mutex);
        m_selectedDeviceId = deviceId;
        return true;
    }

    std::string selectedInputDeviceId() const override
    {
        std::lock_guard lock(m_mutex);
        return m_selectedDeviceId;
    }

    bool startMicrophoneCapture() override
    {
        std::lock_guard lock(m_mutex);
        if (!m_privacyManager.captureStatus().microphoneEnabled) {
            m_lastError.clear();
            return false;
        }
        m_lastError = "Real microphone capture is only implemented on Windows in Phase 3A.";
        return false;
    }

    void stopMicrophoneCapture() override
    {
    }

    bool startSystemAudioCapture() override
    {
        return false;
    }

    void stopSystemAudioCapture() override
    {
    }

    bool isMicrophoneActive() const override
    {
        return false;
    }

    bool isSystemAudioActive() const override
    {
        return false;
    }

    void setTranscriptCallback(TranscriptCallback callback) override
    {
        std::lock_guard lock(m_mutex);
        m_transcriptCallback = std::move(callback);
    }

    double currentInputLevel() const override
    {
        return 0.0;
    }

    MicrophoneDiagnostics diagnostics() const override
    {
        std::lock_guard lock(m_mutex);
        return MicrophoneDiagnostics {
            .selectedDeviceId = m_selectedDeviceId,
            .captureActive = false,
            .lastError = m_lastError
        };
    }

    std::string lastError() const override
    {
        std::lock_guard lock(m_mutex);
        return m_lastError;
    }

private:
    const privacy::PrivacyManager &m_privacyManager;
    mutable std::mutex m_mutex;
    std::string m_selectedDeviceId;
    std::string m_lastError;
    TranscriptCallback m_transcriptCallback;
};
#endif

} // namespace

std::unique_ptr<MicrophoneCapture> createPlatformMicrophoneCapture(
    const privacy::PrivacyManager &privacyManager)
{
#if defined(LOCAL_JARVIS_PLATFORM_WINDOWS)
    return std::make_unique<WindowsMicrophoneCapture>(privacyManager);
#else
    return std::make_unique<UnavailableMicrophoneCapture>(privacyManager);
#endif
}

} // namespace local_jarvis::audio
