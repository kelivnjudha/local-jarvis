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
    , m_asrEngine(local_jarvis::asr::createDefaultAsrEngine())
    , m_modelManager(m_ollamaClient, m_systemCheck)
    , m_companionManager(m_storage)
    , m_captionManager(m_storage)
    , m_setupManager(m_storage, m_modelManager, m_ollamaClient)
    , m_processingQueue(m_storage, m_ollamaClient)
{
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

        refreshStatus();
    });

    connect(m_microphoneCaptureCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_privacyManager.setMicrophoneEnabled(checked);
        if (m_sessionManager) {
            m_sessionManager->syncCaptureWithPrivacy();
        }
        refreshStatus();
    });

    connect(m_systemAudioCaptureCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_privacyManager.setSystemAudioEnabled(checked);
        if (m_sessionManager) {
            m_sessionManager->syncCaptureWithPrivacy();
        }
        refreshStatus();
    });

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

    ensureSessionManager();

    appendLifecycleEvent(QString("Storage ready: %1").arg(pathText(local_jarvis::storage::Storage::defaultDatabasePath())));
}

void MainWindow::ensureSessionManager()
{
    if (m_sessionManager || !m_storage.isOpen()) {
        return;
    }

    m_sessionManager = std::make_unique<local_jarvis::session::SessionManager>(m_storage, m_audioCapture);
    m_sessionManager->setLifecycleCallback([this](const std::string &message) {
        appendLifecycleEvent(QString::fromStdString(message));
    });
    m_sessionManager->setTranscriptCallback([this](const local_jarvis::audio::TranscriptEvent &event) {
        postToUi([this, event]() {
            appendTranscriptLine(event);
            refreshStatus();
        });
    });
    m_processingQueue.setStatusCallback([this](const std::string &message) {
        postToUi([this, message]() {
            m_aiProcessingStatusLabel->setText(QString("AI processing: %1").arg(QString::fromStdString(message)));
            refreshProcessedOutputs();
            refreshStatus();
        });
    });
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
    const QString microphoneRuntime = m_sessionManager && m_sessionManager->isMicrophoneCaptureActive()
        ? "running"
        : "stopped";
    const QString systemAudioRuntime = m_sessionManager && m_sessionManager->isSystemAudioCaptureActive()
        ? "running"
        : "stopped";

    m_captureStatusLabel->setText(QString("Microphone permission: %1 (%2) | System audio permission: %3 (%4) | Screen: %5")
        .arg(enabledText(status.microphoneEnabled),
             microphoneRuntime,
             enabledText(status.systemAudioEnabled),
             systemAudioRuntime,
             enabledText(status.screenCaptureEnabled)));

    const QString asrEngineName = m_asrEngine
        ? QString::fromStdString(m_asrEngine->engineName())
        : "unavailable";
    m_asrStatusLabel->setText(QString("ASR engine: %1").arg(asrEngineName));
    if (m_aiProcessingStatusLabel->text().isEmpty()) {
        m_aiProcessingStatusLabel->setText("AI processing: idle");
    }

    m_startButton->setEnabled(!active && m_sessionManager != nullptr);
    m_stopButton->setEnabled(active);
    const bool modelReady = m_setupStatus.modelReady;
    m_processStudyButton->setEnabled(active && modelReady);
    m_processMeetingButton->setEnabled(active && modelReady);
    m_processFinalSummaryButton->setEnabled(modelReady && (active || m_lastStoppedSessionId.has_value()));
    m_microphoneCaptureCheckBox->setChecked(status.microphoneEnabled);
    m_systemAudioCaptureCheckBox->setChecked(status.systemAudioEnabled);
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
        "Captions: %2 (%3) | Translation: %4\n"
        "Position: %5, %6 | Scale: %7%\n"
        "Theme: %8 | Outfit: %9 | Accessory: %10\n"
        "Animation: %11 | Motion: %12 | Always on top: %13")
        .arg(QString::fromStdString(profile.displayName),
             enabledText(captionState.captionsEnabled && state.captionsVisible),
             QString::fromStdString(local_jarvis::caption::displayName(captionState.captionMode)),
             enabledText(state.translationEnabled),
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
