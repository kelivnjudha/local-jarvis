#include "DummyAudioCapture.h"

#include <algorithm>
#include <string>
#include <utility>

namespace local_jarvis::audio {

DummyAudioCapture::DummyAudioCapture(
    const privacy::PrivacyManager &privacyManager,
    std::chrono::milliseconds interval)
    : m_privacyManager(privacyManager)
    , m_interval(interval)
{
}

DummyAudioCapture::~DummyAudioCapture()
{
    stopWorker();
}

std::vector<AudioInputDevice> DummyAudioCapture::listInputDevices()
{
    return {
        AudioInputDevice {
            .id = "dummy-microphone",
            .displayName = "Dummy microphone",
            .isDefault = true,
            .isAvailable = true
        }
    };
}

bool DummyAudioCapture::selectInputDevice(const std::string &deviceId)
{
    std::lock_guard lock(m_mutex);
    m_selectedDeviceId = deviceId.empty() ? "dummy-microphone" : deviceId;
    return true;
}

std::string DummyAudioCapture::selectedInputDeviceId() const
{
    std::lock_guard lock(m_mutex);
    return m_selectedDeviceId;
}

bool DummyAudioCapture::startMicrophoneCapture()
{
    if (!m_privacyManager.captureStatus().microphoneEnabled) {
        {
            std::lock_guard lock(m_mutex);
            m_lastError.clear();
        }
        stopMicrophoneCapture();
        return false;
    }

    {
        std::lock_guard lock(m_mutex);
        m_microphoneActive = true;
        m_lastError.clear();
    }

    startWorkerIfNeeded();
    return true;
}

void DummyAudioCapture::stopMicrophoneCapture()
{
    {
        std::lock_guard lock(m_mutex);
        m_microphoneActive = false;
    }

    stopWorkerIfIdle();
}

bool DummyAudioCapture::startSystemAudioCapture()
{
    if (!m_privacyManager.captureStatus().systemAudioEnabled) {
        {
            std::lock_guard lock(m_mutex);
            m_lastError.clear();
        }
        stopSystemAudioCapture();
        return false;
    }

    {
        std::lock_guard lock(m_mutex);
        m_systemAudioActive = true;
    }

    startWorkerIfNeeded();
    return true;
}

void DummyAudioCapture::stopSystemAudioCapture()
{
    {
        std::lock_guard lock(m_mutex);
        m_systemAudioActive = false;
    }

    stopWorkerIfIdle();
}

bool DummyAudioCapture::isMicrophoneActive() const
{
    std::lock_guard lock(m_mutex);
    return m_microphoneActive;
}

bool DummyAudioCapture::isSystemAudioActive() const
{
    std::lock_guard lock(m_mutex);
    return m_systemAudioActive;
}

void DummyAudioCapture::setTranscriptCallback(TranscriptCallback callback)
{
    std::lock_guard lock(m_mutex);
    m_transcriptCallback = std::move(callback);
}

double DummyAudioCapture::currentInputLevel() const
{
    std::lock_guard lock(m_mutex);
    if (!m_microphoneActive) {
        return 0.0;
    }

    const double phase = static_cast<double>(m_sequence % 6);
    return std::clamp(0.18 + (phase * 0.05), 0.0, 1.0);
}

std::string DummyAudioCapture::lastError() const
{
    std::lock_guard lock(m_mutex);
    return m_lastError;
}

void DummyAudioCapture::startWorkerIfNeeded()
{
    std::lock_guard lock(m_mutex);
    if (m_worker.joinable()) {
        m_condition.notify_all();
        return;
    }

    m_stopRequested = false;
    m_worker = std::thread([this]() {
        workerLoop();
    });
}

void DummyAudioCapture::stopWorkerIfIdle()
{
    bool shouldStop = false;
    {
        std::lock_guard lock(m_mutex);
        shouldStop = !m_microphoneActive && !m_systemAudioActive;
    }

    if (shouldStop) {
        stopWorker();
    }
}

void DummyAudioCapture::stopWorker()
{
    {
        std::lock_guard lock(m_mutex);
        m_microphoneActive = false;
        m_systemAudioActive = false;
        m_stopRequested = true;
    }

    m_condition.notify_all();

    if (m_worker.joinable()) {
        m_worker.join();
    }

    std::lock_guard lock(m_mutex);
    m_stopRequested = false;
}

void DummyAudioCapture::workerLoop()
{
    while (true) {
        bool microphoneActive = false;
        bool systemAudioActive = false;

        {
            std::unique_lock lock(m_mutex);
            if (m_condition.wait_for(lock, m_interval, [this]() {
                    return m_stopRequested || (!m_microphoneActive && !m_systemAudioActive);
                })) {
                if (m_stopRequested || (!m_microphoneActive && !m_systemAudioActive)) {
                    return;
                }
            }

            microphoneActive = m_microphoneActive;
            systemAudioActive = m_systemAudioActive;
        }

        emitTranscript(microphoneActive, systemAudioActive);
    }
}

void DummyAudioCapture::emitTranscript(bool microphoneActive, bool systemAudioActive)
{
    TranscriptCallback callback;
    std::uint64_t sequence = 0;
    std::int64_t startMs = 0;
    {
        std::lock_guard lock(m_mutex);
        callback = m_transcriptCallback;
        sequence = ++m_sequence;
        startMs = m_nextStartMs;
        m_nextStartMs += m_interval.count();
    }

    if (!callback) {
        return;
    }

    if (microphoneActive) {
        callback(TranscriptEvent {
            .startMs = startMs,
            .endMs = startMs + m_interval.count(),
            .speaker = std::string("Dummy microphone"),
            .text = "Dummy microphone transcript line " + std::to_string(sequence),
            .source = "dummy-microphone"
        });
    }

    if (systemAudioActive) {
        callback(TranscriptEvent {
            .startMs = startMs,
            .endMs = startMs + m_interval.count(),
            .speaker = std::string("Dummy system audio"),
            .text = "Dummy system audio transcript line " + std::to_string(sequence),
            .source = "dummy-system-audio"
        });
    }
}

} // namespace local_jarvis::audio
