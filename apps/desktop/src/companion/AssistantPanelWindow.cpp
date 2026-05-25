#include "companion/AssistantPanelWindow.h"

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

AssistantPanelWindow::AssistantPanelWindow(
    local_jarvis::companion::CompanionManager &companionManager,
    QWidget *parent)
    : QWidget(parent)
    , m_companionManager(companionManager)
{
    setWindowTitle("Local Jarvis Assistant Panel");
    setAccessibleName("Local Jarvis Assistant Panel");
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    setFixedWidth(320);
    buildUi();
    connectSignals();
}

void AssistantPanelWindow::applyState()
{
    const auto &state = m_companionManager.state();
    const QSignalBlocker modeBlocker(m_modeCombo);
    const QSignalBlocker captionsBlocker(m_captionsCheck);
    const QSignalBlocker translationBlocker(m_translationCheck);
    const QSignalBlocker microphoneBlocker(m_microphoneCheck);
    m_modeCombo->setCurrentIndex(static_cast<int>(state.currentMode));
    m_captionsCheck->setChecked(state.captionsVisible);
    m_translationCheck->setChecked(state.translationEnabled);
    m_microphoneCheck->setChecked(state.microphoneEnabled);
    m_statusLabel->setText(QString("Mode: %1 | Animation: %2")
        .arg(QString::fromStdString(toString(state.currentMode)),
             QString::fromStdString(toString(state.currentAnimationState))));
    setVisible(state.companionVisible && state.panelVisible);
}

void AssistantPanelWindow::setAnchorPosition(const QPoint &companionTopLeft)
{
    move(companionTopLeft + QPoint(-width() - 12, 0));
}

void AssistantPanelWindow::setCallbacks(
    std::function<void()> changedCallback,
    std::function<void()> settingsCallback,
    std::function<void()> openFullAppCallback,
    std::function<void()> closeCallback,
    std::function<void()> panelActionCallback)
{
    m_changedCallback = std::move(changedCallback);
    m_settingsCallback = std::move(settingsCallback);
    m_openFullAppCallback = std::move(openFullAppCallback);
    m_closeCallback = std::move(closeCallback);
    m_panelActionCallback = std::move(panelActionCallback);
}

void AssistantPanelWindow::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    auto *title = new QLabel("Local Jarvis", this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(13);
    title->setFont(titleFont);
    rootLayout->addWidget(title);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    rootLayout->addWidget(m_statusLabel);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Study");
    m_modeCombo->addItem("Meeting");
    m_modeCombo->addItem("Interview Practice");
    m_modeCombo->addItem("Review");
    rootLayout->addWidget(m_modeCombo);

    m_captionsCheck = new QCheckBox("Captions", this);
    m_translationCheck = new QCheckBox("Translation", this);
    m_microphoneCheck = new QCheckBox("Microphone placeholder", this);
    rootLayout->addWidget(m_captionsCheck);
    rootLayout->addWidget(m_translationCheck);
    rootLayout->addWidget(m_microphoneCheck);

    auto *buttonLayout = new QHBoxLayout();
    m_pauseButton = new QPushButton("Pause", this);
    m_settingsButton = new QPushButton("Settings", this);
    buttonLayout->addWidget(m_pauseButton);
    buttonLayout->addWidget(m_settingsButton);
    rootLayout->addLayout(buttonLayout);

    m_openFullAppButton = new QPushButton("Open Full App", this);
    m_closeButton = new QPushButton("Collapse", this);
    rootLayout->addWidget(m_openFullAppButton);
    rootLayout->addWidget(m_closeButton);
}

void AssistantPanelWindow::connectSignals()
{
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        m_companionManager.setMode(selectedMode());
        emitPanelAction();
        if (m_changedCallback) {
            m_changedCallback();
        }
    });

    connect(m_captionsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_companionManager.setCaptionsVisible(checked);
        emitPanelAction();
        if (m_changedCallback) {
            m_changedCallback();
        }
    });

    connect(m_translationCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_companionManager.setTranslationEnabled(checked);
        emitPanelAction();
        if (m_changedCallback) {
            m_changedCallback();
        }
    });

    connect(m_microphoneCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_companionManager.setMicrophoneEnabled(checked);
        emitPanelAction();
        if (m_changedCallback) {
            m_changedCallback();
        }
    });

    connect(m_pauseButton, &QPushButton::clicked, this, [this]() {
        emitPanelAction();
    });

    connect(m_settingsButton, &QPushButton::clicked, this, [this]() {
        emitPanelAction();
        if (m_settingsCallback) {
            m_settingsCallback();
        }
    });

    connect(m_openFullAppButton, &QPushButton::clicked, this, [this]() {
        emitPanelAction();
        if (m_openFullAppCallback) {
            m_openFullAppCallback();
        }
    });

    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        m_companionManager.setPanelVisible(false);
        if (m_closeCallback) {
            m_closeCallback();
        }
    });
}

void AssistantPanelWindow::emitPanelAction()
{
    if (m_panelActionCallback) {
        m_panelActionCallback();
    }
}

local_jarvis::companion::CompanionMode AssistantPanelWindow::selectedMode() const
{
    using local_jarvis::companion::CompanionMode;
    switch (m_modeCombo->currentIndex()) {
    case 1:
        return CompanionMode::Meeting;
    case 2:
        return CompanionMode::InterviewPractice;
    case 3:
        return CompanionMode::Review;
    default:
        return CompanionMode::Study;
    }
}
