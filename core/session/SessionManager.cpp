#include "SessionManager.h"

#include "logging/Logger.h"
#include "storage/Storage.h"

#include <chrono>
#include <utility>

namespace local_jarvis::session {

SessionManager::SessionManager(storage::Storage &storage)
    : m_storage(storage)
{
}

SessionManager::SessionManager(storage::Storage &storage, audio::AudioCapture &audioCapture)
    : m_storage(storage)
    , m_audioCapture(&audioCapture)
{
    m_audioCapture->setTranscriptCallback([this](const audio::TranscriptEvent &event) {
        handleTranscriptEvent(event);
    });
}

SessionManager::~SessionManager()
{
    if (m_audioCapture != nullptr) {
        m_audioCapture->stopMicrophoneCapture();
        m_audioCapture->stopSystemAudioCapture();
        m_audioCapture->setTranscriptCallback(nullptr);
    }
}

SessionResult SessionManager::startSession(const std::string &mode)
{
    {
        std::lock_guard lock(m_stateMutex);
        if (m_state == SessionState::Active && m_currentSession.has_value()) {
            return { false, "A session is already active.", m_currentSession };
        }
    }

    const auto sessionId = m_storage.createSession(mode);
    if (!sessionId.has_value()) {
        return { false, "Failed to create session record: " + m_storage.lastError(), std::nullopt };
    }

    SessionInfo session {
        .id = *sessionId,
        .startedAt = std::chrono::system_clock::now(),
        .endedAt = std::nullopt
    };

    {
        std::lock_guard lock(m_stateMutex);
        m_currentSession = session;
        m_state = SessionState::Active;
    }

    const std::string message = "Started session " + session.id;
    emitLifecycleEvent(message);
    syncCaptureWithPrivacy();
    return { true, message, session };
}

SessionResult SessionManager::stopSession()
{
    std::optional<SessionInfo> sessionToStop;
    {
        std::lock_guard lock(m_stateMutex);
        if (m_state == SessionState::Stopped || !m_currentSession.has_value()) {
            return { false, "No active session to stop.", std::nullopt };
        }

        sessionToStop = m_currentSession;
    }

    if (m_audioCapture != nullptr) {
        m_audioCapture->stopMicrophoneCapture();
        m_audioCapture->stopSystemAudioCapture();
    }

    sessionToStop->endedAt = std::chrono::system_clock::now();

    if (!m_storage.endSession(sessionToStop->id)) {
        return { false, "Failed to update session record: " + m_storage.lastError(), sessionToStop };
    }

    const std::string message = "Stopped session " + sessionToStop->id;
    emitLifecycleEvent(message);

    {
        std::lock_guard lock(m_stateMutex);
        m_currentSession.reset();
        m_state = SessionState::Stopped;
    }

    return { true, message, sessionToStop };
}

void SessionManager::syncCaptureWithPrivacy()
{
    if (m_audioCapture == nullptr) {
        return;
    }

    if (state() != SessionState::Active) {
        m_audioCapture->stopMicrophoneCapture();
        m_audioCapture->stopSystemAudioCapture();
        return;
    }

    if (!m_audioCapture->startMicrophoneCapture()) {
        m_audioCapture->stopMicrophoneCapture();
    }

    if (!m_audioCapture->startSystemAudioCapture()) {
        m_audioCapture->stopSystemAudioCapture();
    }
}

std::optional<std::string> SessionManager::currentSessionId() const
{
    std::lock_guard lock(m_stateMutex);
    if (!m_currentSession.has_value()) {
        return std::nullopt;
    }

    return m_currentSession->id;
}

SessionState SessionManager::state() const
{
    std::lock_guard lock(m_stateMutex);
    return m_state;
}

bool SessionManager::isMicrophoneCaptureActive() const
{
    return m_audioCapture != nullptr && m_audioCapture->isMicrophoneActive();
}

bool SessionManager::isSystemAudioCaptureActive() const
{
    return m_audioCapture != nullptr && m_audioCapture->isSystemAudioActive();
}

void SessionManager::setLifecycleCallback(LifecycleCallback callback)
{
    m_lifecycleCallback = std::move(callback);
}

void SessionManager::setTranscriptCallback(TranscriptCallback callback)
{
    m_transcriptCallback = std::move(callback);
}

void SessionManager::emitLifecycleEvent(const std::string &message) const
{
    logging::Logger::instance().info(message);
    if (m_lifecycleCallback) {
        m_lifecycleCallback(message);
    }
}

void SessionManager::handleTranscriptEvent(const audio::TranscriptEvent &event)
{
    std::optional<std::string> sessionId;
    {
        std::lock_guard lock(m_stateMutex);
        if (m_state != SessionState::Active || !m_currentSession.has_value()) {
            return;
        }

        sessionId = m_currentSession->id;
    }

    const auto storedId = m_storage.addTranscriptSegment(storage::TranscriptSegmentInput {
        .sessionId = *sessionId,
        .startMs = event.startMs,
        .endMs = event.endMs,
        .speaker = event.speaker,
        .text = event.text,
        .source = event.source
    });

    if (!storedId.has_value()) {
        logging::Logger::instance().warn("Failed to store transcript segment: " + m_storage.lastError());
        return;
    }

    if (m_transcriptCallback) {
        m_transcriptCallback(event);
    }
}

} // namespace local_jarvis::session
