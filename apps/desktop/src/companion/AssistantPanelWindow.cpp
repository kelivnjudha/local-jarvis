#include "companion/AssistantPanelWindow.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QScreen>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

constexpr int kPanelCompanionOffset = 184;
constexpr int kPanelMargin = 12;

QRect availableDesktopGeometry()
{
    QRect combined;
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        combined = combined.isNull() ? screen->availableGeometry() : combined.united(screen->availableGeometry());
    }
    return combined.isNull() ? QRect(0, 0, 1280, 720) : combined;
}

QPoint clampPanelTopLeft(const QPoint &topLeft, const QSize &size)
{
    const QRect desktop = availableDesktopGeometry().adjusted(kPanelMargin, kPanelMargin, -kPanelMargin, -kPanelMargin);
    return QPoint(
        qBound(desktop.left(), topLeft.x(), qMax(desktop.left(), desktop.right() - size.width())),
        qBound(desktop.top(), topLeft.y(), qMax(desktop.top(), desktop.bottom() - size.height())));
}

} // namespace

AssistantPanelWindow::AssistantPanelWindow(
    local_jarvis::companion::CompanionManager &companionManager,
    local_jarvis::caption::CaptionManager &captionManager,
    QWidget *parent)
    : QWidget(parent)
    , m_companionManager(companionManager)
    , m_captionManager(captionManager)
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
    const auto &captionState = m_captionManager.state();
    const auto profile = m_companionManager.visualProfile();
    const QSignalBlocker modeBlocker(m_modeCombo);
    const QSignalBlocker captionModeBlocker(m_captionModeCombo);
    const QSignalBlocker captionsBlocker(m_captionsCheck);
    const QSignalBlocker showSpeakerBlocker(m_showSpeakerCheck);
    const QSignalBlocker translationBlocker(m_translationCheck);
    const QSignalBlocker microphoneBlocker(m_microphoneCheck);
    const QSignalBlocker asrBlocker(m_asrCheck);
    m_modeCombo->setCurrentIndex(static_cast<int>(state.currentMode));
    m_captionModeCombo->setCurrentIndex(captionModeIndex(captionState.captionMode));
    m_captionsCheck->setChecked(state.captionsVisible && captionState.captionsEnabled);
    m_showSpeakerCheck->setChecked(captionState.showSpeaker);
    m_translationCheck->setChecked(state.translationEnabled);
    m_microphoneCheck->setChecked(state.microphoneEnabled);
    m_asrCheck->setChecked(m_asrEnabled);
    m_languageLabel->setText(QString("Source: Auto | Target: English"));
    m_statusLabel->setText(QString("Mode: %1 | Animation: %2\nOutfit: %3 | Accessory: %4\nASR: %5")
        .arg(QString::fromStdString(profile.displayName),
             QString::fromUtf8(local_jarvis::companion::displayLabelForAnimation(state.currentAnimationState)),
             QString::fromStdString(profile.outfitLabel),
             QString::fromStdString(profile.accessoryLabel),
             m_asrStatusText));
    setStyleSheet(QString(
        "QWidget { background: #f8fafc; color: #17202c; }"
        "QPushButton { min-height: 24px; }"
        "QComboBox { min-height: 24px; }"
        "QCheckBox { min-height: 22px; }"
        "QLabel { border-left: 4px solid %1; padding-left: 8px; }")
            .arg(QString::fromStdString(local_jarvis::companion::colorToHex(profile.primaryColor))));
    applyWindowFlags(state.companionVisible && state.panelVisible);
}

void AssistantPanelWindow::setAsrState(bool enabled, const QString &statusText)
{
    m_asrEnabled = enabled;
    m_asrStatusText = statusText;
}

void AssistantPanelWindow::setAnchorPosition(const QPoint &companionTopLeft)
{
    QPoint candidate = companionTopLeft + QPoint(-width() - kPanelMargin, 0);
    const QRect desktop = availableDesktopGeometry();
    if (candidate.x() < desktop.left() + kPanelMargin) {
        candidate = companionTopLeft + QPoint(kPanelCompanionOffset, 0);
    }
    move(clampPanelTopLeft(candidate, size()));
}

void AssistantPanelWindow::setCallbacks(
    std::function<void()> changedCallback,
    std::function<void()> settingsCallback,
    std::function<void()> openFullAppCallback,
    std::function<void()> closeCallback,
    std::function<void()> panelActionCallback,
    std::function<void(bool)> microphoneToggleCallback,
    std::function<void(bool)> asrToggleCallback)
{
    m_changedCallback = std::move(changedCallback);
    m_settingsCallback = std::move(settingsCallback);
    m_openFullAppCallback = std::move(openFullAppCallback);
    m_closeCallback = std::move(closeCallback);
    m_panelActionCallback = std::move(panelActionCallback);
    m_microphoneToggleCallback = std::move(microphoneToggleCallback);
    m_asrToggleCallback = std::move(asrToggleCallback);
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
    m_modeCombo->setAccessibleName("Companion mode");
    m_modeCombo->addItem("Study");
    m_modeCombo->addItem("Meeting");
    m_modeCombo->addItem("Interview Practice");
    m_modeCombo->addItem("Review");
    rootLayout->addWidget(m_modeCombo);

    m_captionModeCombo = new QComboBox(this);
    m_captionModeCombo->setAccessibleName("Caption mode");
    m_captionModeCombo->addItem("Off");
    m_captionModeCombo->addItem("Original only");
    m_captionModeCombo->addItem("English only");
    m_captionModeCombo->addItem("Original + English");
    m_captionModeCombo->addItem("Summary");
    rootLayout->addWidget(m_captionModeCombo);

    m_captionsCheck = new QCheckBox("Captions", this);
    m_showSpeakerCheck = new QCheckBox("Show speaker", this);
    m_translationCheck = new QCheckBox("Translation", this);
    m_microphoneCheck = new QCheckBox("Microphone", this);
    m_microphoneCheck->setAccessibleName("Assistant microphone toggle");
    m_asrCheck = new QCheckBox("Transcription", this);
    m_asrCheck->setAccessibleName("Assistant ASR transcription toggle");
    rootLayout->addWidget(m_captionsCheck);
    rootLayout->addWidget(m_showSpeakerCheck);
    rootLayout->addWidget(m_translationCheck);
    rootLayout->addWidget(m_microphoneCheck);
    rootLayout->addWidget(m_asrCheck);

    m_languageLabel = new QLabel("Source: Auto | Target: English", this);
    rootLayout->addWidget(m_languageLabel);

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
        m_captionManager.setCaptionsEnabled(checked);
        if (checked && m_captionManager.state().captionMode == local_jarvis::caption::CaptionMode::Off) {
            m_captionManager.setCaptionMode(local_jarvis::caption::CaptionMode::OriginalAndTranslation);
        }
        emitPanelAction();
        if (m_changedCallback) {
            m_changedCallback();
        }
    });

    connect(m_captionModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const auto mode = selectedCaptionMode();
        m_captionManager.setCaptionMode(mode);
        const bool enabled = mode != local_jarvis::caption::CaptionMode::Off;
        m_captionManager.setCaptionsEnabled(enabled);
        m_companionManager.setCaptionsVisible(enabled);
        emitPanelAction();
        if (m_changedCallback) {
            m_changedCallback();
        }
    });

    connect(m_showSpeakerCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_captionManager.setShowSpeaker(checked);
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
        if (m_microphoneToggleCallback) {
            m_microphoneToggleCallback(checked);
        } else {
            m_companionManager.setMicrophoneEnabled(checked);
        }
        if (m_changedCallback) {
            m_changedCallback();
        }
    });

    connect(m_asrCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_asrToggleCallback) {
            m_asrToggleCallback(checked);
        }
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

void AssistantPanelWindow::applyWindowFlags(bool visible)
{
    Qt::WindowFlags flags = Qt::Tool;
    if (m_companionManager.state().alwaysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    if (windowFlags() != flags) {
        setWindowFlags(flags);
    }
    setVisible(visible);
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

local_jarvis::caption::CaptionMode AssistantPanelWindow::selectedCaptionMode() const
{
    using local_jarvis::caption::CaptionMode;
    switch (m_captionModeCombo->currentIndex()) {
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

int AssistantPanelWindow::captionModeIndex(local_jarvis::caption::CaptionMode mode) const
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
