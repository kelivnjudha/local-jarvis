#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>

#include <memory>

#include "ai/ModelManager.h"
#include "ai/OllamaClient.h"
#include "asr/AsrEngine.h"
#include "audio/DummyAudioCapture.h"
#include "privacy/PrivacyManager.h"
#include "session/SessionManager.h"
#include "setup/SetupManager.h"
#include "setup/SystemCheck.h"
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
    void ensureSessionManager();
    void refreshStatus();
    void updateSetupStatus(const local_jarvis::setup::SetupStatus &status);
    void refreshSettings();
    void appendLifecycleEvent(const QString &message);
    void appendSetupLog(const QString &message);
    void appendTranscriptLine(const local_jarvis::audio::TranscriptEvent &event);
    void runSetupAsync();
    void runPullModelAsync(const QString &modelName);

    QTabWidget *m_tabs = nullptr;
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
    QLabel *m_setupDatabaseStatusLabel = nullptr;
    QLabel *m_setupOllamaStatusLabel = nullptr;
    QLabel *m_setupModelStatusLabel = nullptr;
    QPushButton *m_setupButton = nullptr;
    QPushButton *m_setupContinueButton = nullptr;
    QTextEdit *m_setupLogPanel = nullptr;
    QLabel *m_currentModelLabel = nullptr;
    QLineEdit *m_modelNameEdit = nullptr;
    QPushButton *m_changeModelButton = nullptr;
    QPushButton *m_recheckOllamaButton = nullptr;
    QPushButton *m_pullFallbackButton = nullptr;
    QTextEdit *m_deleteModelInstructions = nullptr;

    local_jarvis::privacy::PrivacyManager m_privacyManager;
    local_jarvis::audio::DummyAudioCapture m_audioCapture;
    std::unique_ptr<local_jarvis::asr::AsrEngine> m_asrEngine;
    local_jarvis::ai::OllamaClient m_ollamaClient;
    local_jarvis::setup::SystemCheck m_systemCheck;
    local_jarvis::ai::ModelManager m_modelManager;
    local_jarvis::storage::Storage m_storage;
    local_jarvis::setup::SetupManager m_setupManager;
    local_jarvis::setup::SetupStatus m_setupStatus;
    std::unique_ptr<local_jarvis::session::SessionManager> m_sessionManager;
};
