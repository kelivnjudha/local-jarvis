#include "companion/CaptionBubbleWindow.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QtGlobal>

namespace {

constexpr int kCaptionMinimumWidth = 260;
constexpr int kCaptionMinimumHeight = 80;
constexpr int kCaptionMaximumWidth = 1200;
constexpr int kCaptionMaximumHeight = 500;
constexpr int kResizeHandleSize = 22;
constexpr int kMinimumVisiblePixels = 48;

QRect availableDesktopGeometry()
{
    QRect combined;
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        combined = combined.isNull() ? screen->availableGeometry() : combined.united(screen->availableGeometry());
    }
    return combined.isNull() ? QRect(0, 0, 1280, 720) : combined;
}

QPoint clampTopLeftToDesktop(const QPoint &topLeft, const QSize &size)
{
    const QRect desktop = availableDesktopGeometry();
    const int minimumX = desktop.left() - size.width() + kMinimumVisiblePixels;
    const int maximumX = desktop.right() - kMinimumVisiblePixels;
    const int minimumY = desktop.top();
    const int maximumY = desktop.bottom() - kMinimumVisiblePixels;
    return QPoint(
        qBound(minimumX, topLeft.x(), maximumX),
        qBound(minimumY, topLeft.y(), maximumY));
}

} // namespace

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
    setMouseTracking(true);

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
    if (!m_resizing) {
        const QSize requestedSize(
            qBound(kCaptionMinimumWidth, state.captionWidth, kCaptionMaximumWidth),
            qBound(kCaptionMinimumHeight, state.captionHeight, kCaptionMaximumHeight));
        if (size() != requestedSize) {
            resize(requestedSize);
        }
    }
    if (m_captionLabel) {
        const QRect labelGeometry = rect().adjusted(20, 34, -20, -18);
        if (m_captionLabel->geometry() != labelGeometry) {
            m_captionLabel->setGeometry(labelGeometry);
        }
    }
    updateCaptionLabelStyle();
    if (!qFuzzyCompare(windowOpacity(), state.captionOpacity)) {
        setWindowOpacity(state.captionOpacity);
    }
    if (state.captionDetached && !m_dragging && !m_resizing) {
        const QPoint clampedPosition = clampTopLeftToDesktop(QPoint(state.captionX, state.captionY), size());
        if (pos() != clampedPosition) {
            move(clampedPosition);
        }
        if (clampedPosition.x() != state.captionX || clampedPosition.y() != state.captionY) {
            m_companionManager.setCaptionPosition(clampedPosition.x(), clampedPosition.y());
        }
    }
    applyWindowFlags(state.companionVisible
        && state.captionsVisible
        && captionState.captionsEnabled
        && !m_captionText.trimmed().isEmpty());
    update();
}

void CaptionBubbleWindow::setAnchorPosition(const QPoint &companionTopLeft)
{
    if (m_companionManager.state().captionDetached) {
        return;
    }
    m_companionTopLeft = companionTopLeft;
    const int companionWidth = static_cast<int>(172 * m_companionManager.state().companionScale);
    const QPoint requested = m_companionTopLeft + QPoint(-qMax(0, width() - companionWidth), -height() - 12);
    const QPoint target = clampTopLeftToDesktop(requested, size());
    if (pos() != target) {
        move(target);
    }
}

void CaptionBubbleWindow::setCaptionUpdatedCallback(std::function<void()> callback)
{
    m_captionUpdatedCallback = std::move(callback);
}

void CaptionBubbleWindow::setGeometryChangedCallback(std::function<void()> callback)
{
    m_geometryChangedCallback = std::move(callback);
}

void CaptionBubbleWindow::setMicrophonePlaceholderText(const QString &text)
{
    m_microphonePlaceholderText = text;
    refreshCaptionText();
}

void CaptionBubbleWindow::setDummyCaptionsEnabled(bool enabled)
{
    m_dummyCaptionsEnabled = enabled;
}

void CaptionBubbleWindow::refreshCaptionText()
{
    if (!m_microphonePlaceholderText.trimmed().isEmpty()) {
        m_captionText = m_microphonePlaceholderText;
    } else {
        m_captionText = QString::fromStdString(m_captionManager.currentDisplayText());
    }
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
    painter.drawRoundedRect(dragHandleRect(), 8, 8);

    QFont headerFont = painter.font();
    headerFont.setBold(true);
    headerFont.setPointSize(8);
    painter.setFont(headerFont);
    painter.setPen(foreground);
    painter.drawText(dragHandleRect().adjusted(10, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, "Captions");

    const QRect handle = resizeHandleRect();
    painter.setPen(QPen(border, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(handle.bottomLeft() + QPoint(5, -2), handle.bottomRight() + QPoint(-2, -9));
    painter.drawLine(handle.bottomLeft() + QPoint(11, -2), handle.bottomRight() + QPoint(-2, -3));

    Q_UNUSED(foreground);
}

void CaptionBubbleWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return QWidget::mousePressEvent(event);
    }
    if (m_companionManager.state().captionLocked) {
        return QWidget::mousePressEvent(event);
    }

    m_dragStartGlobal = event->globalPosition().toPoint();
    m_movedDuringDrag = false;
    if (resizeHandleRect().contains(event->position().toPoint())) {
        m_resizing = true;
        m_resizeStartGeometry = geometry();
    } else {
        m_dragging = true;
        m_dragWindowOffset = m_dragStartGlobal - frameGeometry().topLeft();
    }
    event->accept();
}

void CaptionBubbleWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_companionManager.state().captionLocked) {
        return QWidget::mouseMoveEvent(event);
    }

    const QPoint globalPosition = event->globalPosition().toPoint();
    if (m_resizing) {
        const QPoint delta = globalPosition - m_dragStartGlobal;
        const int newWidth = qBound(kCaptionMinimumWidth, m_resizeStartGeometry.width() + delta.x(), kCaptionMaximumWidth);
        const int newHeight = qBound(kCaptionMinimumHeight, m_resizeStartGeometry.height() + delta.y(), kCaptionMaximumHeight);
        resize(newWidth, newHeight);
        if (m_captionLabel) {
            m_captionLabel->setGeometry(rect().adjusted(20, 34, -20, -18));
        }
        m_movedDuringDrag = true;
        update();
        event->accept();
        return;
    }

    if (m_dragging) {
        if ((globalPosition - m_dragStartGlobal).manhattanLength() > 4) {
            m_movedDuringDrag = true;
        }
        move(globalPosition - m_dragWindowOffset);
        event->accept();
        return;
    }

    setCursor(resizeHandleRect().contains(event->position().toPoint()) ? Qt::SizeFDiagCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

void CaptionBubbleWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || (!m_dragging && !m_resizing)) {
        return QWidget::mouseReleaseEvent(event);
    }

    const bool geometryChanged = m_movedDuringDrag;
    m_dragging = false;
    m_resizing = false;
    m_movedDuringDrag = false;
    if (geometryChanged) {
        const QPoint clampedPosition = clampTopLeftToDesktop(pos(), size());
        move(clampedPosition);
        m_companionManager.setCaptionDetached(true);
        m_companionManager.setCaptionGeometry(clampedPosition.x(), clampedPosition.y(), width(), height());
        if (m_geometryChangedCallback) {
            m_geometryChangedCallback();
        }
    }
    event->accept();
}

void CaptionBubbleWindow::updateDummyCaption()
{
    if (!m_dummyCaptionsEnabled) {
        return;
    }

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
    if (isVisible() != visible) {
        setVisible(visible);
    }
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

    const QString styleSheet = QString("background: transparent; color: %1; font-size: %2pt; font-weight: 700;")
        .arg(foreground.name(QColor::HexRgb),
             QString::number(m_companionManager.state().captionFontSize));
    if (m_lastLabelStyleSheet != styleSheet) {
        m_lastLabelStyleSheet = styleSheet;
        m_captionLabel->setStyleSheet(styleSheet);
    }
}

QRect CaptionBubbleWindow::resizeHandleRect() const
{
    return QRect(width() - kResizeHandleSize - 4, height() - kResizeHandleSize - 4, kResizeHandleSize, kResizeHandleSize);
}

QRect CaptionBubbleWindow::dragHandleRect() const
{
    return QRect(12, 10, qMin(120, qMax(92, width() / 3)), 18);
}

QColor CaptionBubbleWindow::toQColor(const local_jarvis::companion::CompanionColor &color) const
{
    return QColor(color.red, color.green, color.blue);
}
