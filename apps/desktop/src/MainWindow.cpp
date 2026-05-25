#include "MainWindow.h"

#include "ai/AiTypes.h"
#include "asr/AsrEngineFactory.h"

#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <string>
#include <thread>

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

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_audioCapture(m_privacyManager)
    , m_asrEngine(local_jarvis::asr::createDefaultAsrEngine())
    , m_modelManager(m_ollamaClient, m_systemCheck)
    , m_setupManager(m_storage, m_modelManager, m_ollamaClient)
{
    buildUi();
    initializeStorage();
    connectSignals();
    refreshStatus();
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
    m_captureStatusLabel->setWordWrap(true);
    statusLayout->addWidget(m_sessionStatusLabel);
    statusLayout->addWidget(m_captureStatusLabel);
    statusLayout->addWidget(m_asrStatusLabel);
    rootLayout->addWidget(statusGroup);

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
        QMetaObject::invokeMethod(this, [this, event]() {
            appendTranscriptLine(event);
            refreshStatus();
        }, Qt::QueuedConnection);
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

    m_startButton->setEnabled(!active && m_sessionManager != nullptr);
    m_stopButton->setEnabled(active);
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

    const bool ready = status.databaseReady && status.modelReady;
    m_setupContinueButton->setEnabled(ready);
    if (m_tabs) {
        m_tabs->setTabEnabled(1, ready);
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
    m_setupButton->setEnabled(false);
    appendSetupLog("Starting user-triggered local AI setup.");

    std::thread([this]() {
        auto status = m_setupManager.setupLocalAi([this](const std::string &line) {
            QMetaObject::invokeMethod(this, [this, line]() {
                appendSetupLog(QString::fromStdString(line));
            }, Qt::QueuedConnection);
        });

        QMetaObject::invokeMethod(this, [this, status]() {
            m_setupStatus = status;
            updateSetupStatus(m_setupStatus);
            ensureSessionManager();
            refreshSettings();
            refreshStatus();
            m_setupButton->setEnabled(true);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::runPullModelAsync(const QString &modelName)
{
    m_pullFallbackButton->setEnabled(false);
    appendSetupLog(QString("Starting user-triggered model pull: %1").arg(modelName));

    std::thread([this, model = modelName.toStdString()]() {
        const bool ok = m_modelManager.pullModel(model, [this](const std::string &line) {
            QMetaObject::invokeMethod(this, [this, line]() {
                appendSetupLog(QString::fromStdString(line));
            }, Qt::QueuedConnection);
        });

        if (ok) {
            m_storage.setSetting("ai.current_model", model);
            m_storage.addModelEvent("fallback_pull_completed", model, "User-triggered fallback model pull completed.");
        } else {
            m_storage.addModelEvent("fallback_pull_failed", model, "User-triggered fallback model pull failed.");
        }

        auto status = m_setupManager.firstRunCheck();
        QMetaObject::invokeMethod(this, [this, status]() {
            m_setupStatus = status;
            updateSetupStatus(m_setupStatus);
            refreshSettings();
            refreshStatus();
            m_pullFallbackButton->setEnabled(true);
        }, Qt::QueuedConnection);
    }).detach();
}
