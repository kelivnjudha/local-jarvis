#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>

#include <memory>

#include "asr/AsrEngine.h"
#include "audio/DummyAudioCapture.h"
#include "privacy/PrivacyManager.h"
#include "session/SessionManager.h"
#include "storage/Storage.h"

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void buildUi();
    void connectSignals();
    void initializeStorage();
    void refreshStatus();
    void appendLifecycleEvent(const QString &message);
    void appendTranscriptLine(const local_jarvis::audio::TranscriptEvent &event);

    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QCheckBox *m_microphoneCaptureCheckBox = nullptr;
    QCheckBox *m_systemAudioCaptureCheckBox = nullptr;
    QLabel *m_sessionStatusLabel = nullptr;
    QLabel *m_captureStatusLabel = nullptr;
    QLabel *m_asrStatusLabel = nullptr;
    QTextEdit *m_transcriptPanel = nullptr;
    QTextEdit *m_notesPanel = nullptr;
    QTextEdit *m_eventLogPanel = nullptr;

    local_jarvis::privacy::PrivacyManager m_privacyManager;
    local_jarvis::audio::DummyAudioCapture m_audioCapture;
    std::unique_ptr<local_jarvis::asr::AsrEngine> m_asrEngine;
    local_jarvis::storage::Storage m_storage;
    std::unique_ptr<local_jarvis::session::SessionManager> m_sessionManager;
};
