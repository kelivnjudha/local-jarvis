#include "MainWindow.h"

#include "ai/AiTypes.h"
#include "asr/AsrEngineFactory.h"
#include "companion/AssistantPanelWindow.h"
#include "companion/CaptionBubbleWindow.h"
#include "companion/CompanionWindow.h"

#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QPoint>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace {

QString enabledText(bool enabled)
{
    return enabled ? "Enabled" : "Disabled";
}

QString pathText(const std::filesystem::path &path)
{
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

int captionModeIndex(local_jarvis::caption::CaptionMode mode)
{
    using local_jarvis::caption::CaptionMode;
    switch (mode) {
    case CaptionMode::Off:
        return 0;
    case CaptionMode::OriginalOnly:
        return 1;
    case CaptionMode::TranslationOnly:
        return 2;
    case CaptionMode::OriginalAndTranslation:
        return 3;
    case CaptionMode::CleanSummary:
        return 4;
    }
    return 3;
}

local_jarvis::caption::CaptionMode captionModeFromIndex(int index)
{
    using local_jarvis::caption::CaptionMode;
    switch (index) {
    case 0:
        return CaptionMode::Off;
    case 1:
        return CaptionMode::OriginalOnly;
    case 2:
        return CaptionMode::TranslationOnly;
    case 4:
        return CaptionMode::CleanSummary;
    default:
        return CaptionMode::OriginalAndTranslation;
    }
}

QString languageDisplay(const std::string &language)
{
    if (language == "auto") {
        return "Auto";
    }
    if (language == "en") {
        return "English";
    }
    return QString::fromStdString(language);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_audioCapture(m_privacyManager)
    , m_asrWorker(std::make_unique<local_jarvis::asr::AsrWorker>(local_jarvis::asr::createDefaultAsrEngine()))
    , m_modelManager(m_ollamaClient, m_systemCheck)
    , m_companionManager(m_storage)
    , m_captionManager(m_storage)
    , m_setupManager(m_storage, m_modelManager, m_ollamaClient)
    , m_processingQueue(m_storage, m_ollamaClient)
{
    m_realMicrophoneCapture = local_jarvis::audio::createPlatformMicrophoneCapture(m_privacyManager);
    m_activeMicrophoneCapture = &m_audioCapture;
    installPcmAudioCallback();
    buildUi();
    initializeStorage();
    initializeCompanion();
    connectSignals();
    refreshStatus();
    refreshCompanionSettings();
}

MainWindow::~MainWindow()
{
    m_destroying.store(true);
    m_microphoneStatusTimer.stop();
    m_microphoneTestTimer.stop();
    stopAsrPipeline();
    clearPcmAudioCallback();
    stopMicrophoneForShutdown();
    m_assistantPanelWindow.reset();
    m_captionBubbleWindow.reset();
    m_companionWindow.reset();
    m_processingQueue.setStatusCallback(nullptr);
    m_sessionManager.reset();
    joinWorkerThreads();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_destroying.store(true);
    m_companionAnimationResetTimer.stop();
    m_microphoneStatusTimer.stop();
    m_microphoneTestTimer.stop();
    stopAsrPipeline();
    clearPcmAudioCallback();
    stopMicrophoneForShutdown();

    if (m_assistantPanelWindow) {
        m_assistantPanelWindow->close();
    }
    if (m_captionBubbleWindow) {
        m_captionBubbleWindow->close();
    }
    if (m_companionWindow) {
        m_companionWindow->close();
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::buildUi()
{
    setWindowTitle("Local Jarvis");
    resize(980, 680);

    m_tabs = new QTabWidget(this);

    auto *setupPage = new QWidget(m_tabs);
    auto *setupLayout = new QVBoxLayout(setupPage);
    setupLayout->setContentsMargins(16, 16, 16, 16);
    setupLayout->setSpacing(12);

    auto *setupStatusGroup = new QGroupBox("First-Run Setup", setupPage);
    auto *setupStatusLayout = new QVBoxLayout(setupStatusGroup);
    m_setupDatabaseStatusLabel = new QLabel(setupStatusGroup);
    m_setupOllamaStatusLabel = new QLabel(setupStatusGroup);
    m_setupModelStatusLabel = new QLabel(setupStatusGroup);
    setupStatusLayout->addWidget(m_setupDatabaseStatusLabel);
    setupStatusLayout->addWidget(m_setupOllamaStatusLabel);
    setupStatusLayout->addWidget(m_setupModelStatusLabel);
    setupLayout->addWidget(setupStatusGroup);

    auto *setupButtonLayout = new QHBoxLayout();
    m_setupButton = new QPushButton("Install / Setup Local AI", setupPage);
    m_setupContinueButton = new QPushButton("Continue", setupPage);
    m_setupContinueButton->setEnabled(false);
    setupButtonLayout->addWidget(m_setupButton);
    setupButtonLayout->addWidget(m_setupContinueButton);
    setupButtonLayout->addStretch();
    setupLayout->addLayout(setupButtonLayout);

    m_setupLogPanel = new QTextEdit(setupPage);
    m_setupLogPanel->setReadOnly(true);
    setupLayout->addWidget(m_setupLogPanel, 1);

    m_tabs->addTab(setupPage, "Setup");

    auto *central = new QWidget(m_tabs);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto *controlsLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start Session", central);
    m_stopButton = new QPushButton("Stop Session", central);
    m_stopButton->setEnabled(false);
    controlsLayout->addWidget(m_startButton);
    controlsLayout->addWidget(m_stopButton);
    m_microphoneCaptureCheckBox = new QCheckBox("Microphone", central);
    m_systemAudioCaptureCheckBox = new QCheckBox("System audio", central);
    controlsLayout->addWidget(m_microphoneCaptureCheckBox);
    controlsLayout->addWidget(m_systemAudioCaptureCheckBox);
    controlsLayout->addStretch();
    rootLayout->addLayout(controlsLayout);

    auto *microphoneGroup = new QGroupBox("Microphone Input", central);
    auto *microphoneLayout = new QVBoxLayout(microphoneGroup);
    auto *microphoneSelectorLayout = new QHBoxLayout();
    m_audioModeCombo = new QComboBox(microphoneGroup);
    m_audioModeCombo->setAccessibleName("Audio capture mode");
    m_audioModeCombo->addItem("Dummy audio", "dummy");
    m_audioModeCombo->addItem("Real microphone", "microphone");
    m_microphoneDeviceCombo = new QComboBox(microphoneGroup);
    m_microphoneDeviceCombo->setAccessibleName("Microphone device");
    m_refreshMicrophoneDevicesButton = new QPushButton("Refresh Devices", microphoneGroup);
    m_microphoneTestButton = new QPushButton("Test Mic Level", microphoneGroup);
    microphoneSelectorLayout->addWidget(m_audioModeCombo);
    microphoneSelectorLayout->addWidget(m_microphoneDeviceCombo, 1);
    microphoneSelectorLayout->addWidget(m_refreshMicrophoneDevicesButton);
    microphoneSelectorLayout->addWidget(m_microphoneTestButton);
    microphoneLayout->addLayout(microphoneSelectorLayout);

    m_microphoneLevelBar = new QProgressBar(microphoneGroup);
    m_microphoneLevelBar->setAccessibleName("Microphone input level");
    m_microphoneLevelBar->setRange(0, 100);
    m_microphoneLevelBar->setValue(0);
    m_microphoneLevelBar->setTextVisible(true);
    microphoneLayout->addWidget(m_microphoneLevelBar);

    auto *microphoneDiagnosticsTitle = new QLabel("Microphone diagnostics", microphoneGroup);
    microphoneLayout->addWidget(microphoneDiagnosticsTitle);
    m_microphoneDiagnosticsLabel = new QLabel(microphoneGroup);
    m_microphoneDiagnosticsLabel->setWordWrap(true);
    m_microphoneDiagnosticsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    microphoneLayout->addWidget(m_microphoneDiagnosticsLabel);

    m_microphoneErrorLabel = new QLabel(microphoneGroup);
    m_microphoneErrorLabel->setWordWrap(true);
    microphoneLayout->addWidget(m_microphoneErrorLabel);
    rootLayout->addWidget(microphoneGroup);

    auto *asrGroup = new QGroupBox("Local ASR", central);
    auto *asrLayout = new QVBoxLayout(asrGroup);
    m_asrEnabledCheckBox = new QCheckBox("Enable local transcription", asrGroup);
    m_asrEnabledCheckBox->setAccessibleName("ASR transcription toggle");
    asrLayout->addWidget(m_asrEnabledCheckBox);
    m_asrBackendLabel = new QLabel(asrGroup);
    m_asrRuntimeStatusLabel = new QLabel(asrGroup);
    m_asrStatsLabel = new QLabel(asrGroup);
    m_asrLastSegmentLabel = new QLabel("Last transcript: none", asrGroup);
    m_asrErrorLabel = new QLabel(asrGroup);
    m_asrRuntimeStatusLabel->setWordWrap(true);
    m_asrStatsLabel->setWordWrap(true);
    m_asrLastSegmentLabel->setWordWrap(true);
    m_asrErrorLabel->setWordWrap(true);
    asrLayout->addWidget(m_asrBackendLabel);
    asrLayout->addWidget(m_asrRuntimeStatusLabel);
    asrLayout->addWidget(m_asrStatsLabel);
    asrLayout->addWidget(m_asrLastSegmentLabel);
    asrLayout->addWidget(m_asrErrorLabel);
    rootLayout->addWidget(asrGroup);

    auto *statusGroup = new QGroupBox("Capture Status", central);
    auto *statusLayout = new QVBoxLayout(statusGroup);
    m_sessionStatusLabel = new QLabel(statusGroup);
    m_captureStatusLabel = new QLabel(statusGroup);
    m_asrStatusLabel = new QLabel(statusGroup);
    m_aiProcessingStatusLabel = new QLabel(statusGroup);
    m_captureStatusLabel->setWordWrap(true);
    statusLayout->addWidget(m_sessionStatusLabel);
    statusLayout->addWidget(m_captureStatusLabel);
    statusLayout->addWidget(m_asrStatusLabel);
    statusLayout->addWidget(m_aiProcessingStatusLabel);
    rootLayout->addWidget(statusGroup);

    auto *processingLayout = new QHBoxLayout();
    m_processStudyButton = new QPushButton("Process Study Chunk", central);
    m_processMeetingButton = new QPushButton("Process Meeting Chunk", central);
    m_processFinalSummaryButton = new QPushButton("Final Summary", central);
    processingLayout->addWidget(m_processStudyButton);
    processingLayout->addWidget(m_processMeetingButton);
    processingLayout->addWidget(m_processFinalSummaryButton);
    processingLayout->addStretch();
    rootLayout->addLayout(processingLayout);

    auto *contentLayout = new QHBoxLayout();

    auto *transcriptGroup = new QGroupBox("Transcript", central);
    auto *transcriptLayout = new QVBoxLayout(transcriptGroup);
    m_transcriptPanel = new QTextEdit(transcriptGroup);
    m_transcriptPanel->setReadOnly(true);
    m_transcriptPanel->setPlaceholderText("Transcript segments will appear here after local ASR is added.");
    transcriptLayout->addWidget(m_transcriptPanel);
    contentLayout->addWidget(transcriptGroup, 1);

    auto *notesGroup = new QGroupBox("Processed Notes", central);
    auto *notesLayout = new QVBoxLayout(notesGroup);
    m_notesPanel = new QTextEdit(notesGroup);
    m_notesPanel->setReadOnly(true);
    m_notesPanel->setPlaceholderText("Local LLM summaries and notes will appear here after processing is added.");
    notesLayout->addWidget(m_notesPanel);
    contentLayout->addWidget(notesGroup, 1);

    rootLayout->addLayout(contentLayout, 1);

    auto *eventGroup = new QGroupBox("Lifecycle Events", central);
    auto *eventLayout = new QVBoxLayout(eventGroup);
    m_eventLogPanel = new QTextEdit(eventGroup);
    m_eventLogPanel->setReadOnly(true);
    eventLayout->addWidget(m_eventLogPanel);
    rootLayout->addWidget(eventGroup);

    m_tabs->addTab(central, "Session");

    auto *settingsPage = new QWidget(m_tabs);
    auto *settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(16, 16, 16, 16);
    settingsLayout->setSpacing(12);

    auto *modelGroup = new QGroupBox("Local Model", settingsPage);
    auto *modelLayout = new QVBoxLayout(modelGroup);
    m_currentModelLabel = new QLabel(modelGroup);
    m_modelNameEdit = new QLineEdit(modelGroup);
    m_modelNameEdit->setPlaceholderText("gemma4:e4b");
    auto *modelButtonLayout = new QHBoxLayout();
    m_changeModelButton = new QPushButton("Change Model", modelGroup);
    m_recheckOllamaButton = new QPushButton("Recheck Ollama", modelGroup);
    m_pullFallbackButton = new QPushButton("Pull Fallback Model", modelGroup);
    modelButtonLayout->addWidget(m_changeModelButton);
    modelButtonLayout->addWidget(m_recheckOllamaButton);
    modelButtonLayout->addWidget(m_pullFallbackButton);
    modelButtonLayout->addStretch();
    modelLayout->addWidget(m_currentModelLabel);
    modelLayout->addWidget(m_modelNameEdit);
    modelLayout->addLayout(modelButtonLayout);
    settingsLayout->addWidget(modelGroup);

    m_deleteModelInstructions = new QTextEdit(settingsPage);
    m_deleteModelInstructions->setReadOnly(true);
    m_deleteModelInstructions->setPlainText("To delete a local model, run this in a terminal:\n\nollama rm gemma4:e4b\n\nLocal Jarvis does not delete models automatically.");
    settingsLayout->addWidget(m_deleteModelInstructions);

    auto *companionGroup = new QGroupBox("Companion", settingsPage);
    auto *companionLayout = new QVBoxLayout(companionGroup);
    m_companionStatusLabel = new QLabel(companionGroup);
    m_companionStatusLabel->setWordWrap(true);
    companionLayout->addWidget(m_companionStatusLabel);

    auto *companionButtonLayout = new QHBoxLayout();
    m_showCompanionButton = new QPushButton("Show Companion", companionGroup);
    m_hideCompanionButton = new QPushButton("Hide Companion", companionGroup);
    m_resetCompanionPositionButton = new QPushButton("Reset Companion Position", companionGroup);
    companionButtonLayout->addWidget(m_showCompanionButton);
    companionButtonLayout->addWidget(m_hideCompanionButton);
    companionButtonLayout->addWidget(m_resetCompanionPositionButton);
    companionButtonLayout->addStretch();
    companionLayout->addLayout(companionButtonLayout);

    m_companionScaleLabel = new QLabel(companionGroup);
    companionLayout->addWidget(m_companionScaleLabel);
    m_companionScaleSlider = new QSlider(Qt::Horizontal, companionGroup);
    m_companionScaleSlider->setRange(75, 140);
    m_companionScaleSlider->setSingleStep(5);
    m_companionScaleSlider->setPageStep(10);
    companionLayout->addWidget(m_companionScaleSlider);

    auto *companionVisualLayout = new QHBoxLayout();
    m_companionAnimationCheckBox = new QCheckBox("Animation enabled", companionGroup);
    m_companionIdleMotionCheckBox = new QCheckBox("Idle motion", companionGroup);
    m_companionAlwaysOnTopCheckBox = new QCheckBox("Always on top", companionGroup);
    companionVisualLayout->addWidget(m_companionAnimationCheckBox);
    companionVisualLayout->addWidget(m_companionIdleMotionCheckBox);
    companionVisualLayout->addWidget(m_companionAlwaysOnTopCheckBox);
    companionVisualLayout->addStretch();
    companionLayout->addLayout(companionVisualLayout);

    m_resetCompanionVisualButton = new QPushButton("Reset Visual Profile / Theme", companionGroup);
    companionLayout->addWidget(m_resetCompanionVisualButton);
    settingsLayout->addWidget(companionGroup);

    auto *captionGroup = new QGroupBox("Captions", settingsPage);
    auto *captionLayout = new QVBoxLayout(captionGroup);
    m_captionModeCombo = new QComboBox(captionGroup);
    m_captionModeCombo->setAccessibleName("Caption mode");
    m_captionModeCombo->addItem("Off");
    m_captionModeCombo->addItem("Original only");
    m_captionModeCombo->addItem("English only");
    m_captionModeCombo->addItem("Original + English");
    m_captionModeCombo->addItem("Summary");
    captionLayout->addWidget(m_captionModeCombo);

    m_captionShowSpeakerCheckBox = new QCheckBox("Show speaker labels", captionGroup);
    m_captionShowSpeakerCheckBox->setAccessibleName("Show speaker labels");
    captionLayout->addWidget(m_captionShowSpeakerCheckBox);

    auto *captionLimitLayout = new QHBoxLayout();
    m_captionMaxLinesSpinBox = new QSpinBox(captionGroup);
    m_captionMaxLinesSpinBox->setAccessibleName("Caption max lines");
    m_captionMaxLinesSpinBox->setRange(1, 8);
    m_captionMaxLinesSpinBox->setPrefix("Max lines: ");
    m_captionMaxCharactersSpinBox = new QSpinBox(captionGroup);
    m_captionMaxCharactersSpinBox->setAccessibleName("Caption max characters");
    m_captionMaxCharactersSpinBox->setRange(40, 1000);
    m_captionMaxCharactersSpinBox->setSingleStep(20);
    m_captionMaxCharactersSpinBox->setPrefix("Max chars: ");
    captionLimitLayout->addWidget(m_captionMaxLinesSpinBox);
    captionLimitLayout->addWidget(m_captionMaxCharactersSpinBox);
    captionLimitLayout->addStretch();
    captionLayout->addLayout(captionLimitLayout);

    auto *captionLanguageLayout = new QHBoxLayout();
    m_captionSourceLanguageEdit = new QLineEdit(captionGroup);
    m_captionSourceLanguageEdit->setAccessibleName("Caption source language");
    m_captionSourceLanguageEdit->setPlaceholderText("auto");
    m_captionTargetLanguageEdit = new QLineEdit(captionGroup);
    m_captionTargetLanguageEdit->setAccessibleName("Caption target language");
    m_captionTargetLanguageEdit->setPlaceholderText("en");
    captionLanguageLayout->addWidget(new QLabel("Source", captionGroup));
    captionLanguageLayout->addWidget(m_captionSourceLanguageEdit);
    captionLanguageLayout->addWidget(new QLabel("Target", captionGroup));
    captionLanguageLayout->addWidget(m_captionTargetLanguageEdit);
    captionLayout->addLayout(captionLanguageLayout);
    settingsLayout->addWidget(captionGroup);

    settingsLayout->addStretch();

    m_tabs->addTab(settingsPage, "Settings");
    m_tabs->setCurrentIndex(0);
    m_tabs->setTabEnabled(1, false);

    setCentralWidget(m_tabs);
}

void MainWindow::connectSignals()
{
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (!m_sessionManager) {
            QMessageBox::critical(this, "Storage Unavailable", "Local Jarvis could not initialize its local database.");
            return;
        }

        const auto result = m_sessionManager->startSession("desktop");
        if (!result.ok) {
            QMessageBox::warning(this, "Session Not Started", QString::fromStdString(result.message));
            return;
        }

        m_lastStoppedSessionId.reset();
        if (result.session.has_value()) {
            m_audioChunkBuffer.reset(result.session->id);
        }
        if (m_asrEnabled.load()) {
            startAsrPipelineIfNeeded();
        }
        refreshStatus();
    });

    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (!m_sessionManager) {
            return;
        }

        const auto result = m_sessionManager->stopSession();
        if (!result.ok) {
            QMessageBox::warning(this, "Session Not Stopped", QString::fromStdString(result.message));
            return;
        }

        if (result.session.has_value()) {
            m_lastStoppedSessionId = result.session->id;
            if (!m_setupStatus.modelReady) {
                m_storage.setSessionSummaryStatus(result.session->id, "waiting_for_model");
                appendLifecycleEvent("Final summary waiting for local model readiness.");
            }
        }

        m_audioChunkBuffer.reset();
        if (m_asrWorker) {
            m_asrWorker->stop();
        }
        refreshStatus();
    });

    connect(m_microphoneCaptureCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        setMicrophoneRequested(checked);
    });

    connect(m_systemAudioCaptureCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && isRealMicrophoneMode()) {
            const QSignalBlocker blocker(m_systemAudioCaptureCheckBox);
            m_systemAudioCaptureCheckBox->setChecked(false);
            appendLifecycleEvent("System audio capture is outside Phase 3A scope.");
            return;
        }
        m_privacyManager.setSystemAudioEnabled(checked);
        if (m_sessionManager) {
            m_sessionManager->syncCaptureWithPrivacy();
        }
        refreshStatus();
    });

    connect(m_audioModeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        handleAudioModeChanged(index);
    });

    connect(m_microphoneDeviceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        handleMicrophoneDeviceChanged(index);
    });

    connect(m_refreshMicrophoneDevicesButton, &QPushButton::clicked, this, [this]() {
        refreshMicrophoneDevices();
    });

    connect(m_microphoneTestButton, &QPushButton::clicked, this, [this]() {
        if (m_microphoneTestActive) {
            stopMicrophoneTest();
        } else {
            startMicrophoneTest();
        }
    });

    connect(m_asrEnabledCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        setAsrEnabled(checked);
    });

    connect(&m_microphoneStatusTimer, &QTimer::timeout, this, [this]() {
        refreshMicrophoneRuntimeUi();
    });
    m_microphoneStatusTimer.start(250);

    connect(&m_microphoneTestTimer, &QTimer::timeout, this, [this]() {
        finishMicrophoneTest();
    });
    m_microphoneTestTimer.setSingleShot(true);

    connect(m_processStudyButton, &QPushButton::clicked, this, [this]() {
        enqueueStudyProcessing();
    });

    connect(m_processMeetingButton, &QPushButton::clicked, this, [this]() {
        enqueueMeetingProcessing();
    });

    connect(m_processFinalSummaryButton, &QPushButton::clicked, this, [this]() {
        enqueueFinalSummaryProcessing();
    });

    connect(m_setupButton, &QPushButton::clicked, this, [this]() {
        runSetupAsync();
    });

    connect(m_setupContinueButton, &QPushButton::clicked, this, [this]() {
        if (m_tabs) {
            m_tabs->setCurrentIndex(1);
        }
    });

    connect(m_changeModelButton, &QPushButton::clicked, this, [this]() {
        const QString modelName = m_modelNameEdit->text().trimmed();
        if (modelName.isEmpty()) {
            return;
        }
        m_storage.setSetting("ai.current_model", modelName.toStdString());
        m_storage.addModelEvent("model_changed", modelName.toStdString(), "User selected current local model.");
        m_setupStatus = m_setupManager.firstRunCheck();
        updateSetupStatus(m_setupStatus);
        refreshSettings();
    });

    connect(m_recheckOllamaButton, &QPushButton::clicked, this, [this]() {
        m_setupStatus = m_setupManager.firstRunCheck();
        updateSetupStatus(m_setupStatus);
        refreshSettings();
    });

    connect(m_pullFallbackButton, &QPushButton::clicked, this, [this]() {
        runPullModelAsync(QString::fromUtf8(local_jarvis::ai::kFallbackGemmaModel));
    });

    connect(m_showCompanionButton, &QPushButton::clicked, this, [this]() {
        showCompanion();
    });

    connect(m_hideCompanionButton, &QPushButton::clicked, this, [this]() {
        hideCompanion();
    });

    connect(m_resetCompanionPositionButton, &QPushButton::clicked, this, [this]() {
        resetCompanionPosition();
    });

    connect(m_companionScaleSlider, &QSlider::valueChanged, this, [this](int value) {
        m_companionManager.setCompanionScale(static_cast<double>(value) / 100.0);
        applyCompanionState();
    });

    connect(m_companionAnimationCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_companionManager.setAnimationEnabled(checked);
        if (checked) {
            setCompanionAnimation(m_companionManager.visualProfile().defaultAnimation);
        } else {
            applyCompanionState();
        }
    });

    connect(m_companionIdleMotionCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_companionManager.setIdleMotionEnabled(checked);
        applyCompanionState();
    });

    connect(m_companionAlwaysOnTopCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_companionManager.setAlwaysOnTop(checked);
        applyCompanionState();
    });

    connect(m_resetCompanionVisualButton, &QPushButton::clicked, this, [this]() {
        resetCompanionVisuals();
    });

    connect(m_captionModeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto mode = captionModeFromIndex(index);
        m_captionManager.setCaptionMode(mode);
        const bool enabled = mode != local_jarvis::caption::CaptionMode::Off;
        m_captionManager.setCaptionsEnabled(enabled);
        m_companionManager.setCaptionsVisible(enabled);
        applyCompanionState();
    });

    connect(m_captionShowSpeakerCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_captionManager.setShowSpeaker(checked);
        applyCompanionState();
    });

    connect(m_captionMaxLinesSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        m_captionManager.setMaxLines(value);
        m_companionManager.setCaptionMaxLines(value);
        applyCompanionState();
    });

    connect(m_captionMaxCharactersSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        m_captionManager.setMaxCharacters(value);
        applyCompanionState();
    });

    connect(m_captionSourceLanguageEdit, &QLineEdit::editingFinished, this, [this]() {
        m_captionManager.setSourceLanguage(m_captionSourceLanguageEdit->text().trimmed().toStdString());
        applyCompanionState();
    });

    connect(m_captionTargetLanguageEdit, &QLineEdit::editingFinished, this, [this]() {
        m_captionManager.setTargetLanguage(m_captionTargetLanguageEdit->text().trimmed().toStdString());
        applyCompanionState();
    });
}

void MainWindow::initializeStorage()
{
    m_setupStatus = m_setupManager.firstRunCheck();
    updateSetupStatus(m_setupStatus);
    refreshSettings();

    if (!m_setupStatus.databaseReady) {
        appendLifecycleEvent(QString("Storage error: %1").arg(QString::fromStdString(m_setupStatus.message)));
        return;
    }

    loadAudioSettings();
    ensureSessionManager();

    appendLifecycleEvent(QString("Storage ready: %1").arg(pathText(local_jarvis::storage::Storage::defaultDatabasePath())));
}

void MainWindow::ensureSessionManager()
{
    if (m_sessionManager || !m_storage.isOpen()) {
        return;
    }

    m_sessionManager = std::make_unique<local_jarvis::session::SessionManager>(m_storage, activeMicrophoneCapture());
    m_sessionManager->setLifecycleCallback([this](const std::string &message) {
        appendLifecycleEvent(QString::fromStdString(message));
    });
    m_sessionManager->setTranscriptCallback([this](const local_jarvis::audio::TranscriptEvent &event) {
        postToUi([this, event]() {
            appendTranscriptLine(event);
            refreshStatus();
        });
    });
    if (m_asrWorker) {
        m_asrWorker->setSegmentCallback([this](const local_jarvis::asr::AsrTranscriptSegment &segment) {
            postToUi([this, segment]() {
                handleAsrTranscriptSegment(segment);
            });
        });
        m_asrWorker->setStatusCallback([this](local_jarvis::asr::AsrStatus status, const std::string &message) {
            postToUi([this, status, message]() {
                if (status == local_jarvis::asr::AsrStatus::Error) {
                    recordAsrEvent("asr_error", message);
                }
                refreshStatus();
            });
        });
    }
    m_processingQueue.setStatusCallback([this](const std::string &message) {
        postToUi([this, message]() {
            m_aiProcessingStatusLabel->setText(QString("AI processing: %1").arg(QString::fromStdString(message)));
            refreshProcessedOutputs();
            refreshStatus();
        });
    });
}

void MainWindow::loadAudioSettings()
{
    if (!m_storage.isOpen()) {
        refreshMicrophoneDevices();
        return;
    }

    const auto mode = m_storage.getSetting("audio.capture_mode").value_or("dummy");
    {
        const QSignalBlocker blocker(m_audioModeCombo);
        m_audioModeCombo->setCurrentIndex(mode == "microphone" ? 1 : 0);
    }
    clearPcmAudioCallback();
    m_activeMicrophoneCapture = isRealMicrophoneMode() && m_realMicrophoneCapture
        ? m_realMicrophoneCapture.get()
        : static_cast<local_jarvis::audio::MicrophoneCapture *>(&m_audioCapture);
    installPcmAudioCallback();

    const auto selectedDevice = m_storage.getSetting("audio.input_device_id").value_or("");
    if (!selectedDevice.empty()) {
        activeMicrophoneCapture().selectInputDevice(selectedDevice);
    }

    const bool asrEnabled = m_storage.getSetting("asr.enabled").value_or("false") == "true";
    m_asrEnabled.store(asrEnabled);
    if (m_asrEnabledCheckBox) {
        const QSignalBlocker blocker(m_asrEnabledCheckBox);
        m_asrEnabledCheckBox->setChecked(asrEnabled);
    }
    if (asrEnabled) {
        startAsrPipelineIfNeeded();
    }

    refreshMicrophoneDevices();
}

void MainWindow::resetSessionManagerForAudioMode()
{
    if (!m_storage.isOpen()) {
        return;
    }

    if (sessionActive()) {
        return;
    }

    if (m_sessionManager) {
        m_sessionManager.reset();
    }
    ensureSessionManager();
}

void MainWindow::refreshMicrophoneDevices()
{
    if (!m_microphoneDeviceCombo) {
        return;
    }

    const QSignalBlocker blocker(m_microphoneDeviceCombo);
    m_microphoneDeviceCombo->clear();

    const auto devices = activeMicrophoneCapture().listInputDevices();
    if (devices.empty()) {
        m_microphoneDeviceCombo->addItem("No microphone devices available", "");
        m_microphoneDeviceCombo->setEnabled(false);
        if (m_microphoneErrorLabel) {
            const auto error = activeMicrophoneCapture().lastError();
            m_microphoneErrorLabel->setText(error.empty()
                    ? "No microphone devices available."
                    : QString::fromStdString(error));
        }
        return;
    }

    const std::string selectedId = activeMicrophoneCapture().selectedInputDeviceId();
    int selectedIndex = 0;
    for (int index = 0; index < static_cast<int>(devices.size()); ++index) {
        const auto &device = devices[static_cast<std::size_t>(index)];
        const QString label = QString("%1%2")
            .arg(QString::fromStdString(device.displayName),
                 device.isDefault ? " (default)" : "");
        m_microphoneDeviceCombo->addItem(label, QString::fromStdString(device.id));
        if (!selectedId.empty() && selectedId == device.id) {
            selectedIndex = index;
        }
    }

    m_microphoneDeviceCombo->setEnabled(true);
    m_microphoneDeviceCombo->setCurrentIndex(selectedIndex);
    activeMicrophoneCapture().selectInputDevice(
        m_microphoneDeviceCombo->currentData().toString().toStdString());
    if (m_microphoneErrorLabel) {
        m_microphoneErrorLabel->clear();
    }
}

void MainWindow::refreshMicrophoneRuntimeUi()
{
    if (!m_microphoneLevelBar) {
        return;
    }

    const auto diagnostics = activeMicrophoneCapture().diagnostics();
    const bool microphoneActive = diagnostics.captureActive;
    const int level = static_cast<int>(std::clamp(diagnostics.smoothedLevel, 0.0, 1.0) * 100.0);
    m_microphoneLevelBar->setValue(level);
    m_microphoneLevelBar->setFormat(microphoneActive ? QString("Mic level: %1%").arg(level) : "Mic level: OFF");

    if (m_microphoneDiagnosticsLabel) {
        m_microphoneDiagnosticsLabel->setText(microphoneDiagnosticsText());
    }

    if (m_microphoneTestButton) {
        m_microphoneTestButton->setText(m_microphoneTestActive ? "Stop Mic Test" : "Test Mic Level");
        m_microphoneTestButton->setEnabled(!sessionActive() || m_microphoneTestActive);
    }

    if (m_microphoneErrorLabel) {
        const QString error = QString::fromStdString(diagnostics.lastError);
        m_microphoneErrorLabel->setText(error);
    }

    if (microphoneActive != m_reportedMicrophoneActive) {
        recordMicrophonePrivacyEvent(
            microphoneActive ? "microphone_enabled" : "microphone_disabled",
            microphoneActive ? "Microphone capture started." : "Microphone capture stopped.");
        m_reportedMicrophoneActive = microphoneActive;
        m_lastReportedMicrophoneFailure.clear();
    }

    const bool wantsMicrophone = m_privacyManager.captureStatus().microphoneEnabled
        && ((m_sessionManager && m_sessionManager->state() == local_jarvis::session::SessionState::Active)
            || m_microphoneTestActive);
    const std::string lastError = diagnostics.lastError;
    if (!microphoneActive && wantsMicrophone && !lastError.empty() && lastError != m_lastReportedMicrophoneFailure) {
        recordMicrophonePrivacyEvent("microphone_start_failed", lastError);
        m_lastReportedMicrophoneFailure = lastError;
    }

    if (m_companionManager.state().microphoneEnabled != microphoneActive) {
        m_companionManager.setMicrophoneEnabled(microphoneActive);
        if (microphoneActive && m_captionBubbleWindow) {
            m_captionBubbleWindow->setMicrophonePlaceholderText(
                m_asrEnabled.load() ? QString {} : QString("Mic active. Transcription is off."));
        } else if (m_captionBubbleWindow) {
            m_captionBubbleWindow->setMicrophonePlaceholderText("");
        }
        applyCompanionState();
    }

    if (m_asrWorker && m_asrEnabled.load()) {
        m_asrWorker->setListening(microphoneActive && sessionActive());
    }

    refreshCompanionSettings();
    refreshStatus();
}

void MainWindow::handleAudioModeChanged(int)
{
    if (sessionActive() || m_microphoneTestActive) {
        const bool activeBackendIsReal = m_realMicrophoneCapture
            && m_activeMicrophoneCapture == m_realMicrophoneCapture.get();
        const QSignalBlocker blocker(m_audioModeCombo);
        m_audioModeCombo->setCurrentIndex(activeBackendIsReal ? 1 : 0);
        QMessageBox::information(this, "Microphone Active", "Stop the current session or microphone test before changing audio capture mode.");
        return;
    }

    clearPcmAudioCallback();
    activeMicrophoneCapture().stopMicrophoneCapture();
    m_activeMicrophoneCapture = isRealMicrophoneMode() && m_realMicrophoneCapture
        ? m_realMicrophoneCapture.get()
        : static_cast<local_jarvis::audio::MicrophoneCapture *>(&m_audioCapture);
    installPcmAudioCallback();

    if (m_storage.isOpen()) {
        m_storage.setSetting("audio.capture_mode", isRealMicrophoneMode() ? "microphone" : "dummy");
    }

    if (isRealMicrophoneMode()) {
        m_privacyManager.setSystemAudioEnabled(false);
    }

    resetSessionManagerForAudioMode();
    refreshMicrophoneDevices();
    refreshStatus();
}

void MainWindow::handleMicrophoneDeviceChanged(int index)
{
    if (index < 0 || !m_microphoneDeviceCombo || !m_microphoneDeviceCombo->isEnabled()) {
        return;
    }

    const std::string deviceId = m_microphoneDeviceCombo->itemData(index).toString().toStdString();
    activeMicrophoneCapture().selectInputDevice(deviceId);
    if (m_storage.isOpen()) {
        m_storage.setSetting("audio.input_device_id", deviceId);
    }
}

void MainWindow::setMicrophoneRequested(bool enabled)
{
    if (m_microphoneTestActive && !enabled) {
        stopMicrophoneTest();
        return;
    }

    m_privacyManager.setMicrophoneEnabled(enabled);
    if (m_sessionManager) {
        m_sessionManager->syncCaptureWithPrivacy();
    }

    if (!enabled) {
        m_lastReportedMicrophoneFailure.clear();
    }

    refreshStatus();
    refreshMicrophoneRuntimeUi();
}

void MainWindow::startMicrophoneTest()
{
    if (sessionActive()) {
        QMessageBox::information(this, "Session Active", "Stop the current session before running the standalone microphone test.");
        return;
    }

    m_microphoneTestPreviousMicRequested = m_privacyManager.captureStatus().microphoneEnabled;
    m_privacyManager.setMicrophoneEnabled(true);
    {
        const QSignalBlocker blocker(m_microphoneCaptureCheckBox);
        m_microphoneCaptureCheckBox->setChecked(true);
    }

    recordMicrophonePrivacyEvent("microphone_test_started", "User started a 10-second microphone level diagnostic test.");
    if (!activeMicrophoneCapture().startMicrophoneCapture()) {
        const std::string error = activeMicrophoneCapture().lastError().empty()
            ? "Microphone test could not start."
            : activeMicrophoneCapture().lastError();
        recordMicrophonePrivacyEvent("microphone_start_failed", error);
        m_privacyManager.setMicrophoneEnabled(m_microphoneTestPreviousMicRequested);
        refreshStatus();
        refreshMicrophoneRuntimeUi();
        return;
    }

    m_microphoneTestActive = true;
    m_microphoneTestTimer.start(10'000);
    appendLifecycleEvent("Microphone diagnostic test running for 10 seconds.");
    refreshStatus();
    refreshMicrophoneRuntimeUi();
}

void MainWindow::stopMicrophoneTest()
{
    if (!m_microphoneTestActive) {
        return;
    }

    m_microphoneTestTimer.stop();
    activeMicrophoneCapture().stopMicrophoneCapture();
    m_microphoneTestActive = false;
    m_privacyManager.setMicrophoneEnabled(m_microphoneTestPreviousMicRequested);
    recordMicrophonePrivacyEvent("microphone_test_stopped", "User stopped the microphone level diagnostic test.");
    refreshStatus();
    refreshMicrophoneRuntimeUi();
}

void MainWindow::finishMicrophoneTest()
{
    if (!m_microphoneTestActive) {
        return;
    }

    activeMicrophoneCapture().stopMicrophoneCapture();
    m_microphoneTestActive = false;
    m_privacyManager.setMicrophoneEnabled(m_microphoneTestPreviousMicRequested);
    recordMicrophonePrivacyEvent("microphone_test_stopped", "Microphone level diagnostic test completed after 10 seconds.");
    refreshStatus();
    refreshMicrophoneRuntimeUi();
}

void MainWindow::setAsrEnabled(bool enabled)
{
    if (m_asrEnabled.exchange(enabled) == enabled) {
        refreshStatus();
        return;
    }

    if (m_storage.isOpen()) {
        m_storage.setSetting("asr.enabled", enabled ? "true" : "false");
    }

    if (m_asrEnabledCheckBox) {
        const QSignalBlocker blocker(m_asrEnabledCheckBox);
        m_asrEnabledCheckBox->setChecked(enabled);
    }

    if (enabled) {
        if (auto sessionId = m_sessionManager ? m_sessionManager->currentSessionId() : std::nullopt) {
            m_audioChunkBuffer.reset(*sessionId);
        }
        startAsrPipelineIfNeeded();
        if (m_captionBubbleWindow) {
            m_captionBubbleWindow->setDummyCaptionsEnabled(false);
            m_captionBubbleWindow->setMicrophonePlaceholderText("");
        }
        if (m_captionManager.state().captionMode == local_jarvis::caption::CaptionMode::Off) {
            m_captionManager.setCaptionMode(local_jarvis::caption::CaptionMode::OriginalOnly);
        }
        m_captionManager.setCaptionsEnabled(true);
        m_companionManager.setCaptionsVisible(true);
        recordAsrEvent("asr_enabled", "Local ASR enabled with backend " + asrBackendText().toStdString() + ".");
    } else {
        m_audioChunkBuffer.reset();
        stopAsrPipeline();
        if (m_captionBubbleWindow) {
            m_captionBubbleWindow->setDummyCaptionsEnabled(true);
        }
        recordAsrEvent("asr_disabled", "Local ASR disabled.");
    }

    refreshStatus();
    applyCompanionState();
}

void MainWindow::startAsrPipelineIfNeeded()
{
    if (!m_asrEnabled.load() || !m_asrWorker) {
        return;
    }

    if (!m_asrWorker->start()) {
        const auto stats = m_asrWorker->stats();
        recordAsrEvent("asr_error", stats.lastError.empty() ? "ASR worker could not start." : stats.lastError);
        return;
    }

    m_asrWorker->setListening(activeMicrophoneCapture().isMicrophoneActive() && sessionActive());
}

void MainWindow::stopAsrPipeline()
{
    if (m_asrWorker) {
        m_asrWorker->stop();
    }
}

void MainWindow::installPcmAudioCallback()
{
    activeMicrophoneCapture().setPcmAudioCallback([this](const local_jarvis::audio::PcmAudioFrame &frame) {
        handlePcmAudioFrame(frame);
    });
}

void MainWindow::clearPcmAudioCallback()
{
    activeMicrophoneCapture().setPcmAudioCallback(nullptr);
}

void MainWindow::handlePcmAudioFrame(const local_jarvis::audio::PcmAudioFrame &frame)
{
    if (m_destroying.load() || !m_asrEnabled.load() || !m_asrWorker || !m_asrWorker->isRunning()) {
        return;
    }

    if (!m_sessionManager || m_sessionManager->state() != local_jarvis::session::SessionState::Active) {
        return;
    }

    const auto sessionId = m_sessionManager->currentSessionId();
    if (!sessionId.has_value()) {
        return;
    }

    auto chunks = m_audioChunkBuffer.appendPcm(
        *sessionId,
        frame.startMs,
        frame.sampleRate,
        frame.channelCount,
        frame.samples);

    for (const auto &chunk : chunks) {
        m_asrWorker->enqueueChunk(chunk);
    }
}

void MainWindow::handleAsrTranscriptSegment(const local_jarvis::asr::AsrTranscriptSegment &segment)
{
    if (m_destroying.load()) {
        return;
    }

    m_captionManager.addSegment(local_jarvis::asr::toCaptionSegment(segment));
    if (m_captionBubbleWindow) {
        m_captionBubbleWindow->setMicrophonePlaceholderText("");
        m_captionBubbleWindow->refreshCaptionText();
        m_captionBubbleWindow->applyState();
    }

    appendTranscriptLine(local_jarvis::audio::TranscriptEvent {
        .startMs = segment.startMs,
        .endMs = segment.endMs,
        .speaker = segment.speaker,
        .text = segment.text,
        .source = asrBackendText().contains("Whisper") ? "microphone_asr_whisper" : "microphone_asr_stub"
    });

    if (m_asrLastSegmentLabel) {
        m_asrLastSegmentLabel->setText(QString("Last transcript: [%1-%2 ms] %3")
            .arg(segment.startMs)
            .arg(segment.endMs)
            .arg(QString::fromStdString(segment.text)));
    }

    const auto currentSessionId = m_sessionManager ? m_sessionManager->currentSessionId() : std::nullopt;
    if (segment.isFinal && currentSessionId.has_value() && *currentSessionId == segment.sessionId) {
        const auto source = asrBackendText().contains("Whisper") ? "microphone_asr_whisper" : "microphone_asr_stub";
        const auto storedId = m_storage.addTranscriptSegment(local_jarvis::storage::TranscriptSegmentInput {
            .sessionId = segment.sessionId,
            .startMs = segment.startMs,
            .endMs = segment.endMs,
            .speaker = segment.speaker,
            .text = segment.text,
            .source = source
        });
        if (!storedId.has_value()) {
            recordAsrEvent("asr_error", "Failed to store ASR transcript segment: " + m_storage.lastError(), segment.sessionId);
        }
    }

    recordAsrEvent("asr_chunk_processed", segment.text, segment.sessionId);
    setCompanionAnimation(local_jarvis::companion::AnimationState::TakingNote);
    refreshStatus();
}

void MainWindow::stopMicrophoneForShutdown()
{
    const bool wasActive = activeMicrophoneCapture().isMicrophoneActive();
    const bool wasTestActive = m_microphoneTestActive;
    m_microphoneTestTimer.stop();
    activeMicrophoneCapture().stopMicrophoneCapture();
    m_microphoneTestActive = false;
    if (wasTestActive) {
        recordMicrophonePrivacyEvent(
            "microphone_test_stopped",
            "Microphone level diagnostic test stopped during app shutdown.");
    }
    if (wasActive) {
        recordMicrophonePrivacyEvent(
            "microphone_disabled",
            "Microphone capture stopped during app shutdown.");
        m_reportedMicrophoneActive = false;
    }
}

void MainWindow::recordMicrophonePrivacyEvent(const std::string &eventType, const std::string &details)
{
    if (!m_storage.isOpen()) {
        return;
    }

    std::optional<std::string> sessionId;
    if (m_sessionManager) {
        sessionId = m_sessionManager->currentSessionId();
    }

    m_storage.addPrivacyEvent(local_jarvis::storage::PrivacyEventInput {
        .sessionId = sessionId,
        .eventType = eventType,
        .details = details
    });

    appendLifecycleEvent(QString("%1: %2")
        .arg(QString::fromStdString(eventType),
             QString::fromStdString(details)));
}

void MainWindow::recordAsrEvent(
    const std::string &eventType,
    const std::string &details,
    const std::optional<std::string> &sessionId)
{
    if (!m_storage.isOpen()) {
        return;
    }

    std::optional<std::string> effectiveSessionId = sessionId;
    if (!effectiveSessionId.has_value() && m_sessionManager) {
        effectiveSessionId = m_sessionManager->currentSessionId();
    }

    m_storage.addPrivacyEvent(local_jarvis::storage::PrivacyEventInput {
        .sessionId = effectiveSessionId,
        .eventType = eventType,
        .details = details
    });

    appendLifecycleEvent(QString("%1: %2")
        .arg(QString::fromStdString(eventType),
             QString::fromStdString(details)));
}

QString MainWindow::microphoneDiagnosticsText() const
{
    const auto diagnostics = activeMicrophoneCapture().diagnostics();
    const QString lastCallback = diagnostics.lastCallbackTimeMs > 0
        ? QDateTime::fromMSecsSinceEpoch(diagnostics.lastCallbackTimeMs, Qt::UTC).toString(Qt::ISODateWithMs)
        : "never";
    const QString deviceName = diagnostics.selectedDeviceName.empty()
        ? (m_microphoneDeviceCombo ? m_microphoneDeviceCombo->currentText() : QString("unknown"))
        : QString::fromStdString(diagnostics.selectedDeviceName);
    const QString deviceId = diagnostics.selectedDeviceId.empty()
        ? QString::fromStdString(activeMicrophoneCapture().selectedInputDeviceId())
        : QString::fromStdString(diagnostics.selectedDeviceId);
    const QString format = diagnostics.sampleFormat.empty()
        ? "unknown"
        : QString::fromStdString(diagnostics.sampleFormat);
    const QString error = diagnostics.lastError.empty()
        ? "none"
        : QString::fromStdString(diagnostics.lastError);

    return QString(
        "Selected device: %1\n"
        "Device id: %2\n"
        "Capture active: %3 | Sample rate: %4 Hz | Channels: %5 | Format: %6\n"
        "Buffers: %7 | Frames: %8 | Non-zero samples: %9\n"
        "Last buffer RMS: %10 | Smoothed level: %11 | Last callback: %12\n"
        "Last error: %13")
        .arg(deviceName,
             deviceId.isEmpty() ? QString("none") : deviceId,
             diagnostics.captureActive ? "yes" : "no",
             QString::number(diagnostics.sampleRate),
             QString::number(diagnostics.channelCount),
             format,
             QString::number(diagnostics.buffersReceived),
             QString::number(diagnostics.framesReceived),
             QString::number(diagnostics.nonZeroSamplesObserved),
             QString::number(diagnostics.lastBufferRms, 'f', 4),
             QString::number(diagnostics.smoothedLevel, 'f', 4),
             lastCallback,
             error);
}

QString MainWindow::asrBackendText() const
{
    if (!m_asrWorker) {
        return "Unavailable";
    }

#if LOCAL_JARVIS_ENABLE_WHISPER
    return QString("%1").arg(QString::fromStdString(m_asrWorker->engineName()));
#else
    return QString("%1").arg(QString::fromStdString(m_asrWorker->engineName()));
#endif
}

QString MainWindow::asrStatusText() const
{
    if (!m_asrWorker) {
        return "Disabled";
    }
    return QString::fromStdString(local_jarvis::asr::toString(m_asrWorker->stats().status));
}

bool MainWindow::sessionActive() const
{
    return m_sessionManager
        && m_sessionManager->state() == local_jarvis::session::SessionState::Active;
}

bool MainWindow::isRealMicrophoneMode() const
{
    return m_audioModeCombo && m_audioModeCombo->currentData().toString() == "microphone";
}

local_jarvis::audio::MicrophoneCapture &MainWindow::activeMicrophoneCapture()
{
    if (m_activeMicrophoneCapture == nullptr) {
        m_activeMicrophoneCapture = &m_audioCapture;
    }
    return *m_activeMicrophoneCapture;
}

const local_jarvis::audio::MicrophoneCapture &MainWindow::activeMicrophoneCapture() const
{
    if (m_activeMicrophoneCapture == nullptr) {
        return m_audioCapture;
    }
    return *m_activeMicrophoneCapture;
}

void MainWindow::refreshStatus()
{
    const bool active = m_sessionManager && m_sessionManager->state() == local_jarvis::session::SessionState::Active;
    const QString sessionId = active
        ? QString::fromStdString(m_sessionManager->currentSessionId().value_or(""))
        : "None";

    m_sessionStatusLabel->setText(QString("Session: %1\nCurrent session ID: %2")
        .arg(active ? "Active" : "Stopped", sessionId));

    const auto status = m_privacyManager.captureStatus();
    const auto microphoneDiagnostics = activeMicrophoneCapture().diagnostics();
    const bool microphoneActive = microphoneDiagnostics.captureActive;
    const QString microphoneRuntime = microphoneActive
        ? "running"
        : "stopped";
    const QString systemAudioRuntime = m_sessionManager && m_sessionManager->isSystemAudioCaptureActive()
        ? "running"
        : "stopped";
    const int microphoneLevel = static_cast<int>(std::clamp(microphoneDiagnostics.smoothedLevel, 0.0, 1.0) * 100.0);
    const QString modeText = isRealMicrophoneMode() ? "Real microphone" : "Dummy audio";
    const QString deviceText = m_microphoneDeviceCombo && m_microphoneDeviceCombo->isEnabled()
        ? m_microphoneDeviceCombo->currentText()
        : "unavailable";

    m_captureStatusLabel->setText(QString("Audio mode: %1 | Device: %2\nMicrophone permission: %3 (%4, level %5%) | System audio permission: %6 (%7) | Screen: %8")
        .arg(modeText,
             deviceText,
             enabledText(status.microphoneEnabled),
             microphoneRuntime,
             QString::number(microphoneLevel),
             enabledText(status.systemAudioEnabled),
             systemAudioRuntime,
             enabledText(status.screenCaptureEnabled)));

    const auto asrStats = m_asrWorker ? m_asrWorker->stats() : local_jarvis::asr::AsrWorkerStats {};
    const QString asrEngineName = asrBackendText();
    m_asrStatusLabel->setText(QString("ASR: %1 | %2")
        .arg(asrEngineName, QString::fromStdString(local_jarvis::asr::toString(asrStats.status))));
    if (m_asrBackendLabel) {
        m_asrBackendLabel->setText(QString("ASR backend: %1").arg(asrEngineName));
    }
    if (m_asrRuntimeStatusLabel) {
        m_asrRuntimeStatusLabel->setText(QString("ASR status: %1 | Toggle: %2")
            .arg(QString::fromStdString(local_jarvis::asr::toString(asrStats.status)),
                 m_asrEnabled.load() ? "ON" : "OFF"));
    }
    if (m_asrStatsLabel) {
        m_asrStatsLabel->setText(QString("Chunks queued: %1 | processed: %2 | pending: %3")
            .arg(QString::number(asrStats.chunksQueued),
                 QString::number(asrStats.chunksProcessed),
                 QString::number(asrStats.pendingChunks)));
    }
    if (m_asrErrorLabel) {
        m_asrErrorLabel->setText(QString("Last ASR error: %1")
            .arg(asrStats.lastError.empty() ? "none" : QString::fromStdString(asrStats.lastError)));
    }
    if (m_aiProcessingStatusLabel->text().isEmpty()) {
        m_aiProcessingStatusLabel->setText("AI processing: idle");
    }

    m_startButton->setEnabled(!active && !m_microphoneTestActive && m_sessionManager != nullptr);
    m_stopButton->setEnabled(active);
    const bool modelReady = m_setupStatus.modelReady;
    m_processStudyButton->setEnabled(active && modelReady);
    m_processMeetingButton->setEnabled(active && modelReady);
    m_processFinalSummaryButton->setEnabled(modelReady && (active || m_lastStoppedSessionId.has_value()));
    {
        const QSignalBlocker microphoneBlocker(m_microphoneCaptureCheckBox);
        const QSignalBlocker systemAudioBlocker(m_systemAudioCaptureCheckBox);
        m_microphoneCaptureCheckBox->setChecked(status.microphoneEnabled);
        m_systemAudioCaptureCheckBox->setChecked(status.systemAudioEnabled);
        m_systemAudioCaptureCheckBox->setEnabled(!isRealMicrophoneMode());
    }
    if (m_microphoneDiagnosticsLabel) {
        m_microphoneDiagnosticsLabel->setText(microphoneDiagnosticsText());
    }
    if (m_microphoneTestButton) {
        m_microphoneTestButton->setText(m_microphoneTestActive ? "Stop Mic Test" : "Test Mic Level");
        m_microphoneTestButton->setEnabled(!active || m_microphoneTestActive);
    }
    if (m_asrEnabledCheckBox) {
        const QSignalBlocker asrBlocker(m_asrEnabledCheckBox);
        m_asrEnabledCheckBox->setChecked(m_asrEnabled.load());
    }
    if (m_audioModeCombo) {
        m_audioModeCombo->setEnabled(!active && !m_microphoneTestActive);
    }
    if (m_microphoneDeviceCombo) {
        m_microphoneDeviceCombo->setEnabled(!active && !m_microphoneTestActive && m_microphoneDeviceCombo->count() > 0);
    }
    if (m_refreshMicrophoneDevicesButton) {
        m_refreshMicrophoneDevicesButton->setEnabled(!active && !m_microphoneTestActive);
    }
}

void MainWindow::updateSetupStatus(const local_jarvis::setup::SetupStatus &status)
{
    m_setupDatabaseStatusLabel->setText(QString("Local database: %1")
        .arg(status.databaseReady ? "ready" : "not ready"));
    m_setupOllamaStatusLabel->setText(QString("Ollama: %1")
        .arg(status.ollamaRunning ? "running locally" : "not running"));
    m_setupModelStatusLabel->setText(QString("Gemma model: %1 (%2)")
        .arg(status.modelReady ? "ready" : "missing",
             QString::fromStdString(status.currentModel.empty() ? status.recommendedModel : status.currentModel)));

    const bool databaseReady = status.databaseReady;
    m_setupContinueButton->setEnabled(databaseReady);
    if (m_tabs) {
        m_tabs->setTabEnabled(1, databaseReady);
    }

    if (!status.message.empty()) {
        appendSetupLog(QString::fromStdString(status.message));
    }
}

void MainWindow::refreshSettings()
{
    const auto currentModel = m_storage.isOpen()
        ? m_storage.getSetting("ai.current_model").value_or(m_modelManager.detectRecommendedModel())
        : m_modelManager.detectRecommendedModel();

    m_currentModelLabel->setText(QString("Current model: %1").arg(QString::fromStdString(currentModel)));
    if (m_modelNameEdit->text().isEmpty()) {
        m_modelNameEdit->setText(QString::fromStdString(currentModel));
    }
}

void MainWindow::initializeCompanion()
{
    if (!m_storage.isOpen()) {
        refreshCompanionSettings();
        return;
    }

    m_companionManager.loadSettings();
    m_captionManager.loadSettings();
    m_companionManager.setCaptionsVisible(m_captionManager.state().captionsEnabled
        && m_captionManager.state().captionMode != local_jarvis::caption::CaptionMode::Off);
    m_companionManager.setCaptionMaxLines(m_captionManager.state().maxLines);
    m_companionWindow = std::make_unique<CompanionWindow>(m_companionManager);
    m_captionBubbleWindow = std::make_unique<CaptionBubbleWindow>(m_companionManager, m_captionManager, m_dummyCaptionSource);
    m_captionBubbleWindow->setDummyCaptionsEnabled(!m_asrEnabled.load());
    m_assistantPanelWindow = std::make_unique<AssistantPanelWindow>(m_companionManager, m_captionManager);

    connect(&m_companionAnimationResetTimer, &QTimer::timeout, this, [this]() {
        m_companionManager.animationStateMachine().onTimeout();
        m_companionManager.syncAnimationState();
        applyCompanionState();
        const auto &state = m_companionManager.state();
        if (state.animationEnabled && state.idleMotionEnabled && state.companionVisible) {
            m_companionAnimationResetTimer.start(1800);
        }
    });
    m_companionAnimationResetTimer.setSingleShot(true);

    m_companionWindow->setClickedCallback([this]() {
        m_companionManager.setPanelVisible(true);
        setCompanionAnimation(local_jarvis::companion::AnimationState::Salute);
    });
    m_companionWindow->setMovedCallback([this](const QPoint &position) {
        m_companionManager.setAnchorPosition(position.x(), position.y());
        setCompanionAnimation(local_jarvis::companion::AnimationState::Walking);
    });
    m_captionBubbleWindow->setCaptionUpdatedCallback([this]() {
        m_companionManager.onCaptionUpdated();
        applyCompanionState();
        scheduleCompanionIdle();
    });
    m_assistantPanelWindow->setCallbacks(
        [this]() {
            applyCompanionState();
        },
        [this]() {
            setCompanionAnimation(local_jarvis::companion::AnimationState::Working);
            if (m_tabs) {
                m_tabs->setCurrentIndex(2);
            }
            show();
            raise();
            activateWindow();
        },
        [this]() {
            setCompanionAnimation(local_jarvis::companion::AnimationState::Working);
            show();
            raise();
            activateWindow();
        },
        [this]() {
            applyCompanionState();
        },
        [this]() {
            setCompanionAnimation(local_jarvis::companion::AnimationState::Working);
        },
        [this](bool enabled) {
            setMicrophoneRequested(enabled);
        },
        [this](bool enabled) {
            setAsrEnabled(enabled);
        });

    applyCompanionState();
}

void MainWindow::applyCompanionState()
{
    m_companionManager.syncAnimationState();
    const auto &state = m_companionManager.state();
    const QPoint anchor(state.anchorX, state.anchorY);

    if (m_companionWindow) {
        m_companionWindow->applyState();
    }
    if (m_captionBubbleWindow) {
        m_captionBubbleWindow->refreshCaptionText();
        m_captionBubbleWindow->applyState();
        m_captionBubbleWindow->setAnchorPosition(anchor);
    }
    if (m_assistantPanelWindow) {
        m_assistantPanelWindow->setAsrState(
            m_asrEnabled.load(),
            QString("%1 (%2)")
                .arg(asrStatusText(), asrBackendText()));
        m_assistantPanelWindow->applyState();
        m_assistantPanelWindow->setAnchorPosition(anchor);
    }

    refreshCompanionSettings();
    refreshCaptionSettings();
}

void MainWindow::refreshCompanionSettings()
{
    if (!m_companionStatusLabel) {
        return;
    }

    const auto &state = m_companionManager.state();
    const auto &captionState = m_captionManager.state();
    const auto profile = m_companionManager.visualProfile();
    m_companionStatusLabel->setText(QString(
        "Mode: %1\n"
        "Captions: %2 (%3) | Translation: %4 | Mic: %5\n"
        "Position: %6, %7 | Scale: %8%\n"
        "Theme: %9 | Outfit: %10 | Accessory: %11\n"
        "Animation: %12 | Motion: %13 | Always on top: %14")
        .arg(QString::fromStdString(profile.displayName),
             enabledText(captionState.captionsEnabled && state.captionsVisible),
             QString::fromStdString(local_jarvis::caption::displayName(captionState.captionMode)),
             enabledText(state.translationEnabled),
             state.microphoneEnabled ? "ON" : "OFF",
             QString::number(state.anchorX),
             QString::number(state.anchorY),
             QString::number(static_cast<int>(state.companionScale * 100.0)),
             QString::fromStdString(state.themePack),
             QString::fromStdString(profile.outfitLabel),
             QString::fromStdString(profile.accessoryLabel),
             QString::fromUtf8(local_jarvis::companion::displayLabelForAnimation(state.currentAnimationState)),
             enabledText(state.animationEnabled && state.idleMotionEnabled),
             enabledText(state.alwaysOnTop)));

    const QSignalBlocker scaleBlocker(m_companionScaleSlider);
    const QSignalBlocker animationBlocker(m_companionAnimationCheckBox);
    const QSignalBlocker idleMotionBlocker(m_companionIdleMotionCheckBox);
    const QSignalBlocker alwaysOnTopBlocker(m_companionAlwaysOnTopCheckBox);
    m_companionScaleLabel->setText(QString("Companion scale: %1%")
        .arg(static_cast<int>(state.companionScale * 100.0)));
    m_companionScaleSlider->setValue(static_cast<int>(state.companionScale * 100.0));
    m_companionAnimationCheckBox->setChecked(state.animationEnabled);
    m_companionIdleMotionCheckBox->setChecked(state.idleMotionEnabled);
    m_companionAlwaysOnTopCheckBox->setChecked(state.alwaysOnTop);
}

void MainWindow::refreshCaptionSettings()
{
    if (!m_captionModeCombo) {
        return;
    }

    const auto &state = m_captionManager.state();
    const QSignalBlocker modeBlocker(m_captionModeCombo);
    const QSignalBlocker speakerBlocker(m_captionShowSpeakerCheckBox);
    const QSignalBlocker linesBlocker(m_captionMaxLinesSpinBox);
    const QSignalBlocker charactersBlocker(m_captionMaxCharactersSpinBox);
    const QSignalBlocker sourceBlocker(m_captionSourceLanguageEdit);
    const QSignalBlocker targetBlocker(m_captionTargetLanguageEdit);

    m_captionModeCombo->setCurrentIndex(captionModeIndex(state.captionMode));
    m_captionShowSpeakerCheckBox->setChecked(state.showSpeaker);
    m_captionMaxLinesSpinBox->setValue(state.maxLines);
    m_captionMaxCharactersSpinBox->setValue(state.maxCharacters);
    m_captionSourceLanguageEdit->setText(QString::fromStdString(state.sourceLanguage));
    m_captionTargetLanguageEdit->setText(QString::fromStdString(state.targetLanguage));
}

void MainWindow::setCompanionAnimation(local_jarvis::companion::AnimationState state)
{
    m_companionManager.setAnimationState(state);
    applyCompanionState();
    if (m_companionManager.state().animationEnabled) {
        scheduleCompanionIdle();
    }
}

void MainWindow::scheduleCompanionIdle()
{
    if (m_companionManager.state().animationEnabled) {
        m_companionAnimationResetTimer.start(1400);
    }
}

void MainWindow::showCompanion()
{
    m_companionManager.setCompanionVisible(true);
    setCompanionAnimation(local_jarvis::companion::AnimationState::Salute);
}

void MainWindow::hideCompanion()
{
    m_companionManager.setPanelVisible(false);
    m_companionManager.setCompanionVisible(false);
    applyCompanionState();
}

void MainWindow::resetCompanionPosition()
{
    m_companionManager.resetAnchorPosition();
    m_companionManager.setCompanionVisible(true);
    setCompanionAnimation(local_jarvis::companion::AnimationState::Walking);
}

void MainWindow::resetCompanionVisuals()
{
    m_companionManager.resetVisualSettings();
    setCompanionAnimation(m_companionManager.visualProfile().defaultAnimation);
}

void MainWindow::refreshProcessedOutputs()
{
    if (!m_sessionManager || !m_notesPanel) {
        return;
    }

    auto sessionId = m_sessionManager->currentSessionId();
    if (!sessionId.has_value()) {
        sessionId = m_lastStoppedSessionId;
    }
    if (!sessionId.has_value()) {
        return;
    }

    QString output;
    const auto notes = m_storage.listLatestProcessedNotes(*sessionId, 5);
    if (!notes.empty()) {
        output += "Processed notes\n";
        for (const auto &note : notes) {
            output += QString("[%1] %2\n%3\n\n")
                .arg(QString::fromStdString(note.type),
                     QString::fromStdString(note.title.value_or("Untitled")),
                     QString::fromStdString(note.body));
        }
    }

    const auto flashcards = m_storage.listFlashcards(*sessionId, 10);
    if (!flashcards.empty()) {
        output += "Flashcards\n";
        for (const auto &flashcard : flashcards) {
            output += QString("Q: %1\nA: %2\n\n")
                .arg(QString::fromStdString(flashcard.question),
                     QString::fromStdString(flashcard.answer));
        }
    }

    const auto actionItems = m_storage.listActionItems(*sessionId, 10);
    if (!actionItems.empty()) {
        output += "Action items\n";
        for (const auto &item : actionItems) {
            output += QString("[%1] %2\n")
                .arg(QString::fromStdString(item.status),
                     QString::fromStdString(item.text));
        }
    }

    if (!output.isEmpty()) {
        m_notesPanel->setPlainText(output.trimmed());
    }
}

void MainWindow::appendLifecycleEvent(const QString &message)
{
    if (!m_eventLogPanel) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_eventLogPanel->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::appendSetupLog(const QString &message)
{
    if (!m_setupLogPanel || message.trimmed().isEmpty()) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_setupLogPanel->append(QString("[%1] %2").arg(timestamp, message.trimmed()));
}

void MainWindow::appendTranscriptLine(const local_jarvis::audio::TranscriptEvent &event)
{
    if (!m_transcriptPanel) {
        return;
    }

    const QString speaker = event.speaker.has_value()
        ? QString::fromStdString(*event.speaker)
        : "Unknown";
    m_transcriptPanel->append(QString("[%1-%2 ms] %3: %4")
        .arg(event.startMs)
        .arg(event.endMs)
        .arg(speaker, QString::fromStdString(event.text)));
}

void MainWindow::runSetupAsync()
{
    joinFinishedWorkers();
    if (m_setupWorkerActive.exchange(true)) {
        appendSetupLog("Local AI setup is already running.");
        return;
    }

    m_setupButton->setEnabled(false);
    appendSetupLog("Starting user-triggered local AI setup.");

    m_setupThread = std::thread([this]() {
        auto status = m_setupManager.setupLocalAi([this](const std::string &line) {
            postToUi([this, line]() {
                appendSetupLog(QString::fromStdString(line));
            });
        });

        m_setupWorkerActive.store(false);
        postToUi([this, status]() {
            m_setupStatus = status;
            updateSetupStatus(m_setupStatus);
            ensureSessionManager();
            refreshSettings();
            refreshStatus();
            m_setupButton->setEnabled(true);
        });
    });
}

void MainWindow::runPullModelAsync(const QString &modelName)
{
    joinFinishedWorkers();
    if (m_modelPullWorkerActive.exchange(true)) {
        appendSetupLog("Model pull is already running.");
        return;
    }

    m_pullFallbackButton->setEnabled(false);
    appendSetupLog(QString("Starting user-triggered model pull: %1").arg(modelName));

    m_modelPullThread = std::thread([this, model = modelName.toStdString()]() {
        const bool ok = m_modelManager.pullModel(model, [this](const std::string &line) {
            postToUi([this, line]() {
                appendSetupLog(QString::fromStdString(line));
            });
        });

        if (ok) {
            m_storage.setSetting("ai.current_model", model);
            m_storage.addModelEvent("fallback_pull_completed", model, "User-triggered fallback model pull completed.");
        } else {
            m_storage.addModelEvent("fallback_pull_failed", model, "User-triggered fallback model pull failed.");
        }

        auto status = m_setupManager.firstRunCheck();
        m_modelPullWorkerActive.store(false);
        postToUi([this, status]() {
            m_setupStatus = status;
            updateSetupStatus(m_setupStatus);
            refreshSettings();
            refreshStatus();
            m_pullFallbackButton->setEnabled(true);
        });
    });
}

void MainWindow::enqueueStudyProcessing()
{
    if (!m_sessionManager) {
        return;
    }

    const auto sessionId = m_sessionManager->currentSessionId();
    if (!sessionId.has_value()) {
        return;
    }

    m_aiProcessingStatusLabel->setText("AI processing: queued study chunk");
    m_processingQueue.enqueueStudyChunk(*sessionId);
}

void MainWindow::enqueueMeetingProcessing()
{
    if (!m_sessionManager) {
        return;
    }

    const auto sessionId = m_sessionManager->currentSessionId();
    if (!sessionId.has_value()) {
        return;
    }

    m_aiProcessingStatusLabel->setText("AI processing: queued meeting chunk");
    m_processingQueue.enqueueMeetingChunk(*sessionId);
}

void MainWindow::enqueueFinalSummaryProcessing()
{
    if (!m_sessionManager) {
        return;
    }

    auto sessionId = m_sessionManager->currentSessionId();
    if (!sessionId.has_value()) {
        sessionId = m_lastStoppedSessionId;
    }
    if (!sessionId.has_value()) {
        return;
    }

    if (!m_setupStatus.modelReady) {
        m_storage.setSessionSummaryStatus(*sessionId, "waiting_for_model");
        m_aiProcessingStatusLabel->setText("AI processing: waiting for local model");
        return;
    }

    m_aiProcessingStatusLabel->setText("AI processing: queued final summary");
    m_processingQueue.enqueueFinalSessionSummary(*sessionId);
}

void MainWindow::postToUi(std::function<void()> callback)
{
    if (m_destroying.load()) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, callback = std::move(callback)]() mutable {
        if (m_destroying.load()) {
            return;
        }
        callback();
    }, Qt::QueuedConnection);
}

void MainWindow::joinFinishedWorkers()
{
    if (!m_setupWorkerActive.load() && m_setupThread.joinable()) {
        m_setupThread.join();
    }
    if (!m_modelPullWorkerActive.load() && m_modelPullThread.joinable()) {
        m_modelPullThread.join();
    }
}

void MainWindow::joinWorkerThreads()
{
    if (m_setupThread.joinable()) {
        m_setupThread.join();
    }
    if (m_modelPullThread.joinable()) {
        m_modelPullThread.join();
    }
}
