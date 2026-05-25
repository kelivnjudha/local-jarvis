#pragma once

#include "audio/AudioCapture.h"
#include "SessionTypes.h"

#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace local_jarvis::storage {
class Storage;
}

namespace local_jarvis::session {

class SessionManager {
public:
    using LifecycleCallback = std::function<void(const std::string &)>;
    using TranscriptCallback = std::function<void(const audio::TranscriptEvent &)>;

    explicit SessionManager(storage::Storage &storage);
    SessionManager(storage::Storage &storage, audio::AudioCapture &audioCapture);
    ~SessionManager();

    SessionResult startSession(const std::string &mode = "manual");
    SessionResult stopSession();
    void syncCaptureWithPrivacy();

    [[nodiscard]] std::optional<std::string> currentSessionId() const;
    [[nodiscard]] SessionState state() const;
    [[nodiscard]] bool isMicrophoneCaptureActive() const;
    [[nodiscard]] bool isSystemAudioCaptureActive() const;

    void setLifecycleCallback(LifecycleCallback callback);
    void setTranscriptCallback(TranscriptCallback callback);

private:
    void emitLifecycleEvent(const std::string &message) const;
    void handleTranscriptEvent(const audio::TranscriptEvent &event);

    storage::Storage &m_storage;
    audio::AudioCapture *m_audioCapture = nullptr;
    mutable std::mutex m_stateMutex;
    SessionState m_state = SessionState::Stopped;
    std::optional<SessionInfo> m_currentSession;
    LifecycleCallback m_lifecycleCallback;
    TranscriptCallback m_transcriptCallback;
};

} // namespace local_jarvis::session
