#pragma once

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "ai/ModelManager.h"
#include "ai/OllamaClient.h"
#include "asr/AsrEngine.h"
#include "audio/DummyAudioCapture.h"
#include "caption/CaptionManager.h"
#include "caption/DummyCaptionSource.h"
#include "companion/CompanionManager.h"
#include "privacy/PrivacyManager.h"
#include "processing/ProcessingQueue.h"
#include "session/SessionManager.h"
#include "setup/SetupManager.h"
#include "setup/SystemCheck.h"
#include "storage/Storage.h"

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void connectSignals();
    void initializeStorage();
    void ensureSessionManager();
    void refreshStatus();
    void updateSetupStatus(const local_jarvis::setup::SetupStatus &status);
    void refreshSettings();
    void initializeCompanion();
    void applyCompanionState();
    void refreshCompanionSettings();
    void refreshCaptionSettings();
    void setCompanionAnimation(local_jarvis::companion::AnimationState state);
    void scheduleCompanionIdle();
    void showCompanion();
    void hideCompanion();
    void resetCompanionPosition();
    void resetCompanionVisuals();
    void refreshProcessedOutputs();
    void appendLifecycleEvent(const QString &message);
    void appendSetupLog(const QString &message);
    void appendTranscriptLine(const local_jarvis::audio::TranscriptEvent &event);
    void runSetupAsync();
    void runPullModelAsync(const QString &modelName);
    void enqueueStudyProcessing();
    void enqueueMeetingProcessing();
    void enqueueFinalSummaryProcessing();
    void postToUi(std::function<void()> callback);
    void joinFinishedWorkers();
    void joinWorkerThreads();

    QTabWidget *m_tabs = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QCheckBox *m_microphoneCaptureCheckBox = nullptr;
    QCheckBox *m_systemAudioCaptureCheckBox = nullptr;
    QLabel *m_sessionStatusLabel = nullptr;
    QLabel *m_captureStatusLabel = nullptr;
    QLabel *m_asrStatusLabel = nullptr;
    QLabel *m_aiProcessingStatusLabel = nullptr;
    QPushButton *m_processStudyButton = nullptr;
    QPushButton *m_processMeetingButton = nullptr;
    QPushButton *m_processFinalSummaryButton = nullptr;
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
    QLabel *m_companionStatusLabel = nullptr;
    QPushButton *m_showCompanionButton = nullptr;
    QPushButton *m_hideCompanionButton = nullptr;
    QPushButton *m_resetCompanionPositionButton = nullptr;
    QLabel *m_companionScaleLabel = nullptr;
    QSlider *m_companionScaleSlider = nullptr;
    QCheckBox *m_companionAnimationCheckBox = nullptr;
    QCheckBox *m_companionIdleMotionCheckBox = nullptr;
    QCheckBox *m_companionAlwaysOnTopCheckBox = nullptr;
    QPushButton *m_resetCompanionVisualButton = nullptr;
    QComboBox *m_captionModeCombo = nullptr;
    QCheckBox *m_captionShowSpeakerCheckBox = nullptr;
    QSpinBox *m_captionMaxLinesSpinBox = nullptr;
    QSpinBox *m_captionMaxCharactersSpinBox = nullptr;
    QLineEdit *m_captionSourceLanguageEdit = nullptr;
    QLineEdit *m_captionTargetLanguageEdit = nullptr;

    local_jarvis::privacy::PrivacyManager m_privacyManager;
    local_jarvis::audio::DummyAudioCapture m_audioCapture;
    std::unique_ptr<local_jarvis::asr::AsrEngine> m_asrEngine;
    local_jarvis::ai::OllamaClient m_ollamaClient;
    local_jarvis::setup::SystemCheck m_systemCheck;
    local_jarvis::ai::ModelManager m_modelManager;
    local_jarvis::storage::Storage m_storage;
    local_jarvis::companion::CompanionManager m_companionManager;
    local_jarvis::caption::CaptionManager m_captionManager;
    local_jarvis::caption::DummyCaptionSource m_dummyCaptionSource;
    local_jarvis::setup::SetupManager m_setupManager;
    local_jarvis::setup::SetupStatus m_setupStatus;
    local_jarvis::processing::ProcessingQueue m_processingQueue;
    std::unique_ptr<local_jarvis::session::SessionManager> m_sessionManager;
    std::unique_ptr<class CompanionWindow> m_companionWindow;
    std::unique_ptr<class CaptionBubbleWindow> m_captionBubbleWindow;
    std::unique_ptr<class AssistantPanelWindow> m_assistantPanelWindow;
    std::optional<std::string> m_lastStoppedSessionId;
    std::thread m_setupThread;
    std::thread m_modelPullThread;
    QTimer m_companionAnimationResetTimer;
    std::atomic_bool m_destroying { false };
    std::atomic_bool m_setupWorkerActive { false };
    std::atomic_bool m_modelPullWorkerActive { false };
};
