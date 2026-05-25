#include "companion/CaptionBubbleWindow.h"

#include <QPainter>
#include <QtGlobal>

CaptionBubbleWindow::CaptionBubbleWindow(
    local_jarvis::companion::CompanionManager &companionManager,
    local_jarvis::caption::CaptionManager &captionManager,
    local_jarvis::caption::DummyCaptionSource &dummyCaptionSource,
    QWidget *parent)
    : QWidget(parent)
    , m_companionManager(companionManager)
    , m_captionManager(captionManager)
    , m_dummyCaptionSource(dummyCaptionSource)
{
    setWindowTitle("Local Jarvis Captions");
    setAccessibleName("Local Jarvis Caption Bubble");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    m_captionLabel = new QLabel(this);
    m_captionLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_captionLabel->setWordWrap(true);
    m_captionLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    connect(&m_captionTimer, &QTimer::timeout, this, [this]() {
        updateDummyCaption();
    });
    m_captionTimer.start(3200);
    updateDummyCaption();
}

void CaptionBubbleWindow::applyState()
{
    const auto &state = m_companionManager.state();
    const auto &captionState = m_captionManager.state();
    refreshCaptionText();
    resize(state.captionWidth, qMax(80, state.captionFontSize * (captionState.maxLines + 2)));
    if (m_captionLabel) {
        m_captionLabel->setGeometry(rect().adjusted(20, 18, -20, -14));
    }
    updateCaptionLabelStyle();
    setWindowOpacity(state.captionOpacity);
    applyWindowFlags(state.companionVisible
        && state.captionsVisible
        && captionState.captionsEnabled
        && !m_captionText.trimmed().isEmpty());
    update();
}

void CaptionBubbleWindow::setAnchorPosition(const QPoint &companionTopLeft)
{
    m_companionTopLeft = companionTopLeft;
    const int companionWidth = static_cast<int>(172 * m_companionManager.state().companionScale);
    move(m_companionTopLeft + QPoint(-qMax(0, width() - companionWidth), -height() - 12));
}

void CaptionBubbleWindow::setCaptionUpdatedCallback(std::function<void()> callback)
{
    m_captionUpdatedCallback = std::move(callback);
}

void CaptionBubbleWindow::refreshCaptionText()
{
    m_captionText = QString::fromStdString(m_captionManager.currentDisplayText());
    if (m_captionLabel) {
        m_captionLabel->setText(m_captionText);
    }
    setAccessibleDescription(m_captionText);
    setToolTip(m_captionText);
}

void CaptionBubbleWindow::paintEvent(QPaintEvent *)
{
    const auto profile = m_companionManager.visualProfile();
    const QColor primary = toQColor(profile.primaryColor);
    const QColor accent = toQColor(profile.accentColor);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::NoPen);
    QColor background(20, 24, 30, 235);
    QColor foreground(238, 242, 248);
    QColor border = accent;
    if (profile.captionBubbleStyle == "note-card") {
        background = QColor(251, 249, 232, 240);
        foreground = QColor(34, 45, 40);
        border = primary;
    } else if (profile.captionBubbleStyle == "meeting-caption") {
        background = QColor(235, 242, 252, 240);
        foreground = QColor(30, 42, 62);
        border = primary;
    } else if (profile.captionBubbleStyle == "coaching-prompt") {
        background = QColor(255, 242, 229, 242);
        foreground = QColor(54, 39, 25);
        border = primary;
    } else if (profile.captionBubbleStyle == "reading-review") {
        background = QColor(243, 238, 252, 242);
        foreground = QColor(39, 34, 54);
        border = primary;
    }

    painter.setBrush(background);
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 14, 14);

    painter.setPen(QPen(border, 3));
    painter.drawRoundedRect(rect().adjusted(2, 2, -3, -3), 12, 12);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(border.red(), border.green(), border.blue(), 70));
    painter.drawRoundedRect(QRect(12, 10, 92, 18), 8, 8);

    Q_UNUSED(foreground);
}

void CaptionBubbleWindow::updateDummyCaption()
{
    m_captionManager.addSegment(m_dummyCaptionSource.nextSegment());
    refreshCaptionText();
    ++m_captionIndex;
    update();

    if (m_captionUpdatedCallback) {
        m_captionUpdatedCallback();
    }
}

void CaptionBubbleWindow::applyWindowFlags(bool visible)
{
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint;
    if (m_companionManager.state().alwaysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    if (windowFlags() != flags) {
        setWindowFlags(flags);
    }
    setVisible(visible);
}

void CaptionBubbleWindow::updateCaptionLabelStyle()
{
    if (!m_captionLabel) {
        return;
    }

    const auto profile = m_companionManager.visualProfile();
    QColor foreground(238, 242, 248);
    if (profile.captionBubbleStyle == "note-card") {
        foreground = QColor(34, 45, 40);
    } else if (profile.captionBubbleStyle == "meeting-caption") {
        foreground = QColor(30, 42, 62);
    } else if (profile.captionBubbleStyle == "coaching-prompt") {
        foreground = QColor(54, 39, 25);
    } else if (profile.captionBubbleStyle == "reading-review") {
        foreground = QColor(39, 34, 54);
    }

    m_captionLabel->setStyleSheet(QString("background: transparent; color: %1; font-size: %2pt; font-weight: 700;")
        .arg(foreground.name(QColor::HexRgb),
             QString::number(m_companionManager.state().captionFontSize)));
}

QColor CaptionBubbleWindow::toQColor(const local_jarvis::companion::CompanionColor &color) const
{
    return QColor(color.red, color.green, color.blue);
}
