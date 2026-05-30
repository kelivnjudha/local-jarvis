#pragma once

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
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
#include <vector>

#include "ai/ModelManager.h"
#include "ai/OllamaClient.h"
#include "asr/AsrTypes.h"
#include "asr/AsrWorker.h"
#include "asr/AudioChunkBuffer.h"
#include "audio/DummyAudioCapture.h"
#include "audio/MicrophoneCapture.h"
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
    void loadAudioSettings();
    void resetSessionManagerForAudioMode();
    void refreshStatus();
    void refreshMicrophoneDevices();
    void refreshMicrophoneRuntimeUi();
    void handleAudioModeChanged(int index);
    void handleMicrophoneDeviceChanged(int index);
    void setMicrophoneRequested(bool enabled);
    void startMicrophoneTest();
    void stopMicrophoneTest();
    void finishMicrophoneTest();
    void startMicrophoneDeviceCompare();
    void stopMicrophoneDeviceCompare();
    void advanceMicrophoneDeviceCompare();
    void startCurrentCompareDevice();
    void recordCurrentCompareSample(const local_jarvis::audio::MicrophoneDiagnostics &diagnostics);
    void finishMicrophoneDeviceCompare(bool canceled);
    void loadAsrSettings();
    void saveAsrSettings();
    void setAsrEnabled(bool enabled);
    void handleAsrBackendChanged(int index);
    void handleWhisperSettingsChanged();
    void browseWhisperModelPath();
    void resetAsrWorkerForBackend();
    void configureAsrWorkerCallbacks();
    void startAsrPipelineIfNeeded();
    void stopAsrPipeline();
    void installPcmAudioCallback();
    void clearPcmAudioCallback();
    void handlePcmAudioFrame(const local_jarvis::audio::PcmAudioFrame &frame);
    void handleAsrTranscriptSegment(const local_jarvis::asr::AsrTranscriptSegment &segment);
    void stopMicrophoneForShutdown();
    void recordMicrophonePrivacyEvent(const std::string &eventType, const std::string &details);
    void recordAsrEvent(const std::string &eventType, const std::string &details, const std::optional<std::string> &sessionId = std::nullopt);
    [[nodiscard]] QString microphoneDiagnosticsText() const;
    [[nodiscard]] QString microphoneDeviceListText() const;
    [[nodiscard]] QString microphoneRecommendationText() const;
    [[nodiscard]] QString asrBackendText() const;
    [[nodiscard]] QString asrStatusText() const;
    [[nodiscard]] QString whisperStatusText() const;
    [[nodiscard]] local_jarvis::asr::AsrBackend selectedAsrBackend() const;
    [[nodiscard]] local_jarvis::asr::AsrEngineConfig currentAsrConfig() const;
    [[nodiscard]] std::string currentAsrTranscriptSource() const;
    [[nodiscard]] bool sessionActive() const;
    [[nodiscard]] bool isRealMicrophoneMode() const;
    [[nodiscard]] local_jarvis::audio::MicrophoneCapture &activeMicrophoneCapture();
    [[nodiscard]] const local_jarvis::audio::MicrophoneCapture &activeMicrophoneCapture() const;
    void updateSetupStatus(const local_jarvis::setup::SetupStatus &status);
    void refreshSettings();
    void initializeCompanion();
    void applyCompanionState(bool repositionPanel = false);
    void refreshCompanionSettings();
    void refreshCaptionSettings();
    void setCompanionAnimation(local_jarvis::companion::AnimationState state);
    void scheduleCompanionIdle();
    void toggleAssistantPanel();
    void positionAssistantPanelNearRobot();
    void openAssistantPanel();
    void closeAssistantPanel();
    void resetRobotPosition();
    void resetCaptionPlacement();
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
    QComboBox *m_audioModeCombo = nullptr;
    QComboBox *m_microphoneDeviceCombo = nullptr;
    QPushButton *m_refreshMicrophoneDevicesButton = nullptr;
    QPushButton *m_microphoneTestButton = nullptr;
    QPushButton *m_compareMicrophoneDevicesButton = nullptr;
    QProgressBar *m_microphoneLevelBar = nullptr;
    QLabel *m_microphoneRecommendationLabel = nullptr;
    QLabel *m_microphoneDiagnosticsLabel = nullptr;
    QLabel *m_microphoneErrorLabel = nullptr;
    QComboBox *m_asrBackendCombo = nullptr;
    QCheckBox *m_asrEnabledCheckBox = nullptr;
    QLineEdit *m_whisperModelPathEdit = nullptr;
    QPushButton *m_whisperBrowseButton = nullptr;
    QComboBox *m_whisperLanguageCombo = nullptr;
    QCheckBox *m_whisperTranslateCheckBox = nullptr;
    QSpinBox *m_whisperThreadsSpinBox = nullptr;
    QLabel *m_asrBackendLabel = nullptr;
    QLabel *m_asrRuntimeStatusLabel = nullptr;
    QLabel *m_whisperStatusLabel = nullptr;
    QLabel *m_asrStatsLabel = nullptr;
    QLabel *m_asrLastSegmentLabel = nullptr;
    QLabel *m_asrErrorLabel = nullptr;
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
    QPushButton *m_openAssistantPanelButton = nullptr;
    QPushButton *m_closeAssistantPanelButton = nullptr;
    QPushButton *m_resetRobotPositionButton = nullptr;
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
    QCheckBox *m_captionLockedCheckBox = nullptr;
    QPushButton *m_resetCaptionPositionButton = nullptr;
    QSpinBox *m_captionWidthSpinBox = nullptr;
    QSpinBox *m_captionHeightSpinBox = nullptr;
    QSpinBox *m_captionFontSizeSpinBox = nullptr;
    QDoubleSpinBox *m_captionOpacitySpinBox = nullptr;

    local_jarvis::privacy::PrivacyManager m_privacyManager;
    local_jarvis::audio::DummyAudioCapture m_audioCapture;
    std::unique_ptr<local_jarvis::audio::MicrophoneCapture> m_realMicrophoneCapture;
    local_jarvis::audio::MicrophoneCapture *m_activeMicrophoneCapture = nullptr;
    std::unique_ptr<local_jarvis::asr::AsrWorker> m_asrWorker;
    local_jarvis::asr::AudioChunkBuffer m_audioChunkBuffer;
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
    QTimer m_microphoneStatusTimer;
    QTimer m_microphoneTestTimer;
    QTimer m_microphoneCompareTimer;
    std::atomic_bool m_destroying { false };
    std::atomic_bool m_setupWorkerActive { false };
    std::atomic_bool m_modelPullWorkerActive { false };
    std::atomic_bool m_asrEnabled { false };
    local_jarvis::asr::AsrBackend m_asrBackend = local_jarvis::asr::AsrBackend::Stub;
    std::string m_whisperModelPath;
    std::string m_whisperLanguage = "auto";
    bool m_whisperTranslateToEnglish = false;
    int m_whisperMaxThreads = 4;
    bool m_whisperLoadInProgress = false;
    bool m_reportedMicrophoneActive = false;
    bool m_microphoneTestActive = false;
    bool m_microphoneTestPreviousMicRequested = false;
    struct MicrophoneDeviceSnapshot {
        QString name;
        std::string id;
        bool isDefault = false;
    };
    struct MicrophoneCompareResult {
        QString name;
        std::string id;
        double bestRms = 0.0;
        double bestPeak = 0.0;
        double bestDbfs = -120.0;
        double bestNonZeroRatio = 0.0;
        bool started = false;
    };
    std::vector<MicrophoneDeviceSnapshot> m_microphoneDevices;
    std::vector<int> m_microphoneCompareIndices;
    std::vector<MicrophoneCompareResult> m_microphoneCompareResults;
    bool m_microphoneCompareActive = false;
    bool m_microphoneComparePreviousMicRequested = false;
    int m_microphoneCompareOriginalIndex = -1;
    int m_microphoneCompareCurrentPosition = -1;
    int m_microphoneCompareIntervalMs = 1000;
    MicrophoneCompareResult m_microphoneCompareCurrentResult;
    QString m_microphoneRecommendation;
    double m_asrQuietRmsThreshold = 0.01;
    std::string m_lastReportedMicrophoneFailure;
};
