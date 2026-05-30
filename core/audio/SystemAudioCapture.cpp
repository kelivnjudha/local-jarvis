#include "SystemAudioCapture.h"

#if defined(LOCAL_JARVIS_PLATFORM_WINDOWS)
#include "SystemAudioCapture_Windows.h"
#endif

#include <mutex>
#include <utility>

namespace local_jarvis::audio {
namespace {

#if !defined(LOCAL_JARVIS_PLATFORM_WINDOWS)
class UnavailableSystemAudioCapture final : public SystemAudioCapture {
public:
    explicit UnavailableSystemAudioCapture(const privacy::PrivacyManager &privacyManager)
        : m_privacyManager(privacyManager)
    {
    }

    std::vector<AudioOutputDevice> listOutputDevices() override
    {
        return {};
    }

    bool selectOutputDevice(const std::string &deviceId) override
    {
        std::lock_guard lock(m_mutex);
        m_selectedDeviceId = deviceId;
        m_diagnostics.selectedDeviceId = deviceId;
        return true;
    }

    std::string selectedOutputDeviceId() const override
    {
        std::lock_guard lock(m_mutex);
        return m_selectedDeviceId;
    }

    bool startSystemAudioCapture() override
    {
        std::lock_guard lock(m_mutex);
        if (!m_privacyManager.captureStatus().systemAudioEnabled) {
            m_lastError.clear();
            return false;
        }
        m_lastError = "System audio loopback capture is only implemented on Windows in Phase 3E-A.";
        m_diagnostics.lastError = m_lastError;
        return false;
    }

    void stopSystemAudioCapture() override
    {
        std::lock_guard lock(m_mutex);
        m_diagnostics.captureActive = false;
        m_diagnostics.smoothedLevel = 0.0;
    }

    void setPcmAudioCallback(PcmAudioCallback callback) override
    {
        std::lock_guard lock(m_mutex);
        m_pcmAudioCallback = std::move(callback);
    }

    bool isSystemAudioActive() const override
    {
        return false;
    }

    double currentOutputLevel() const override
    {
        return 0.0;
    }

    SystemAudioDiagnostics diagnostics() const override
    {
        std::lock_guard lock(m_mutex);
        auto diagnostics = m_diagnostics;
        diagnostics.selectedDeviceId = m_selectedDeviceId;
        diagnostics.captureActive = false;
        diagnostics.smoothedLevel = 0.0;
        diagnostics.lastError = m_lastError;
        return diagnostics;
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
    SystemAudioDiagnostics m_diagnostics;
    PcmAudioCallback m_pcmAudioCallback;
};
#endif

} // namespace

std::unique_ptr<SystemAudioCapture> createPlatformSystemAudioCapture(
    const privacy::PrivacyManager &privacyManager)
{
#if defined(LOCAL_JARVIS_PLATFORM_WINDOWS)
    return std::make_unique<WindowsSystemAudioCapture>(privacyManager);
#else
    return std::make_unique<UnavailableSystemAudioCapture>(privacyManager);
#endif
}

} // namespace local_jarvis::audio
