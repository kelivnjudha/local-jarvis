#include "MainWindow.h"

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

    auto *central = new QWidget(this);
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

    setCentralWidget(central);
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
}

void MainWindow::initializeStorage()
{
    const std::filesystem::path storagePath = local_jarvis::storage::Storage::defaultDatabasePath();

    if (!m_storage.open(storagePath)) {
        appendLifecycleEvent(QString("Storage error: %1").arg(QString::fromStdString(m_storage.lastError())));
        return;
    }

    if (!m_storage.createSchema()) {
        appendLifecycleEvent(QString("Schema error: %1").arg(QString::fromStdString(m_storage.lastError())));
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
    appendLifecycleEvent(QString("Storage ready: %1").arg(pathText(storagePath)));
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

void MainWindow::appendLifecycleEvent(const QString &message)
{
    if (!m_eventLogPanel) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_eventLogPanel->append(QString("[%1] %2").arg(timestamp, message));
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
