#include "companion/CompanionWindow.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QtMath>
#include <QtGlobal>

namespace {

constexpr int kBaseWindowWidth = 172;
constexpr int kBaseWindowHeight = 212;
constexpr int kMinimumVisiblePixels = 48;
constexpr int kDragClickThreshold = 6;

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

CompanionWindow::CompanionWindow(
    local_jarvis::companion::CompanionManager &companionManager,
    QWidget *parent)
    : QWidget(parent)
    , m_companionManager(companionManager)
{
    setWindowTitle("Local Jarvis Companion");
    setAccessibleName("Local Jarvis Companion");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kBaseWindowWidth, kBaseWindowHeight);
    setMouseTracking(true);

    connect(&m_animationTimer, &QTimer::timeout, this, [this]() {
        ++m_frame;
        update();
    });
    m_animationTimer.start(80);
}

void CompanionWindow::applyState()
{
    const auto &state = m_companionManager.state();
    if (!m_dragging) {
        const int scaledWidth = static_cast<int>(kBaseWindowWidth * state.companionScale);
        const int scaledHeight = static_cast<int>(kBaseWindowHeight * state.companionScale);
        const QSize scaledSize(scaledWidth, scaledHeight);
        if (size() != scaledSize) {
            setFixedSize(scaledSize);
        }
    }
    if (state.animationEnabled) {
        const auto metadata = local_jarvis::companion::metadataForAnimation(state.currentAnimationState);
        m_animationTimer.start(qMax(60, metadata.transitionDurationMs / 3));
    } else {
        m_animationTimer.stop();
    }
    if (!m_dragging) {
        const QPoint clampedPosition = clampTopLeftToDesktop(QPoint(state.anchorX, state.anchorY), size());
        move(clampedPosition);
        if (clampedPosition.x() != state.anchorX || clampedPosition.y() != state.anchorY) {
            m_companionManager.setAnchorPosition(clampedPosition.x(), clampedPosition.y());
        }
    }
    applyWindowFlags(true);
    update();
}

void CompanionWindow::setClickedCallback(std::function<void()> callback)
{
    m_clickedCallback = std::move(callback);
}

void CompanionWindow::setMovedCallback(std::function<void(const QPoint &)> callback)
{
    m_movedCallback = std::move(callback);
}

void CompanionWindow::paintEvent(QPaintEvent *)
{
    const auto &state = m_companionManager.state();
    const auto profile = m_companionManager.visualProfile();
    const auto animation = state.currentAnimationState;
    const double scale = state.companionScale;
    const int bob = state.animationEnabled ? static_cast<int>(qSin(m_frame / 8.0) * 4.0 * scale) : 0;
    const int pulse = state.animationEnabled ? static_cast<int>(qSin(m_frame / 6.0) * 3.0 * scale) : 0;
    const int yOffset = animation == local_jarvis::companion::AnimationState::Walking ? bob : 0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(scale, scale);

    const QColor primary = toQColor(profile.primaryColor);
    const QColor accent = toQColor(profile.accentColor);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(18, 22, 28, 225));
    painter.drawRoundedRect(QRect(8, 12, kBaseWindowWidth - 16, kBaseWindowHeight - 20), 18, 18);

    painter.setBrush(QColor(primary.red(), primary.green(), primary.blue(), 48));
    painter.drawEllipse(QPoint(kBaseWindowWidth / 2, 78 + yOffset), 52 + pulse, 58 + pulse);

    painter.setBrush(primary);
    painter.drawEllipse(QPoint(kBaseWindowWidth / 2, 54 + yOffset), 30, 30);

    painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 210));
    painter.drawRoundedRect(QRect(kBaseWindowWidth / 2 - 32, 82 + yOffset, 64, 44), 15, 15);

    painter.setBrush(QColor(245, 248, 252));
    painter.drawEllipse(QPoint(kBaseWindowWidth / 2 - 10, 50 + yOffset), 5, 7);
    painter.drawEllipse(QPoint(kBaseWindowWidth / 2 + 10, 50 + yOffset), 5, 7);

    if (state.currentMode == local_jarvis::companion::CompanionMode::Study) {
        painter.setPen(QPen(QColor(20, 24, 30), 2));
        painter.drawLine(QPoint(kBaseWindowWidth / 2 - 16, 50 + yOffset), QPoint(kBaseWindowWidth / 2 + 16, 50 + yOffset));
        painter.drawEllipse(QPoint(kBaseWindowWidth / 2 - 10, 51 + yOffset), 8, 6);
        painter.drawEllipse(QPoint(kBaseWindowWidth / 2 + 10, 51 + yOffset), 8, 6);
    }

    painter.setPen(QPen(QColor(245, 248, 252), 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRect(kBaseWindowWidth / 2 - 12, 58 + yOffset, 24, 16), 200 * 16, 140 * 16);

    painter.setPen(QPen(accent.lighter(120), 8, Qt::SolidLine, Qt::RoundCap));
    const QPoint leftShoulder(kBaseWindowWidth / 2 - 28, 84 + yOffset);
    const QPoint rightShoulder(kBaseWindowWidth / 2 + 28, 84 + yOffset);
    painter.drawLine(leftShoulder, QPoint(kBaseWindowWidth / 2 - 48, 112 + yOffset));
    if (animation == local_jarvis::companion::AnimationState::Salute) {
        painter.drawLine(rightShoulder, QPoint(kBaseWindowWidth / 2 + 38, 42 + yOffset));
    } else {
        painter.drawLine(rightShoulder, QPoint(kBaseWindowWidth / 2 + 48, 112 + yOffset));
    }

    painter.setPen(QPen(primary.lighter(145), 7, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPoint(kBaseWindowWidth / 2 - 16, 118 + yOffset), QPoint(kBaseWindowWidth / 2 - 22, 140));
    painter.drawLine(QPoint(kBaseWindowWidth / 2 + 16, 118 + yOffset), QPoint(kBaseWindowWidth / 2 + 22, 140));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(242, 244, 248, 235));
    if (state.currentMode == local_jarvis::companion::CompanionMode::Meeting) {
        painter.drawRoundedRect(QRect(112, 80 + yOffset, 24, 34), 4, 4);
        painter.setBrush(primary);
        painter.drawRect(QRect(117, 87 + yOffset, 14, 18));
    } else if (state.currentMode == local_jarvis::companion::CompanionMode::InterviewPractice) {
        painter.drawRoundedRect(QRect(112, 82 + yOffset, 30, 20), 4, 4);
        painter.setBrush(primary);
        painter.drawRect(QRect(117, 88 + yOffset, 20, 2));
        painter.drawRect(QRect(117, 94 + yOffset, 14, 2));
    } else if (state.currentMode == local_jarvis::companion::CompanionMode::Review) {
        painter.drawRoundedRect(QRect(34, 86 + yOffset, 26, 24), 4, 4);
        painter.setBrush(primary);
        painter.drawRect(QRect(38, 90 + yOffset, 8, 16));
        painter.drawRect(QRect(48, 90 + yOffset, 8, 16));
    } else {
        painter.drawRoundedRect(QRect(112, 84 + yOffset, 24, 28), 5, 5);
        painter.setBrush(primary);
        painter.drawRect(QRect(117, 91 + yOffset, 14, 2));
        painter.drawRect(QRect(117, 97 + yOffset, 14, 2));
    }

    painter.setPen(QColor(235, 238, 244));
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(9);
    painter.setFont(font);
    painter.drawText(QRect(14, 138, kBaseWindowWidth - 28, 18), Qt::AlignCenter, QString::fromStdString(profile.displayName));

    font.setPointSize(8);
    font.setBold(false);
    painter.setFont(font);
    painter.setPen(QColor(185, 194, 206));
    painter.drawText(QRect(12, 156, kBaseWindowWidth - 24, 15), Qt::AlignCenter, QString::fromStdString(profile.outfitLabel));
    painter.drawText(QRect(12, 172, kBaseWindowWidth - 24, 15), Qt::AlignCenter, QString::fromStdString(profile.accessoryLabel));

    painter.setPen(QColor(214, 221, 230));
    font.setPointSize(8);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(
        QRect(12, 190, kBaseWindowWidth - 24, 14),
        Qt::AlignCenter,
        QString::fromUtf8(local_jarvis::companion::displayLabelForAnimation(state.currentAnimationState)));
}

void CompanionWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return QWidget::mousePressEvent(event);
    }

    m_dragging = true;
    m_movedDuringDrag = false;
    m_dragStartGlobal = event->globalPosition().toPoint();
    m_dragWindowOffset = m_dragStartGlobal - frameGeometry().topLeft();
    event->accept();
}

void CompanionWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || m_companionManager.state().companionLocked) {
        return QWidget::mouseMoveEvent(event);
    }

    const QPoint globalPosition = event->globalPosition().toPoint();
    if ((globalPosition - m_dragStartGlobal).manhattanLength() > kDragClickThreshold) {
        m_movedDuringDrag = true;
    }
    move(globalPosition - m_dragWindowOffset);
    event->accept();
}

void CompanionWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging) {
        return QWidget::mouseReleaseEvent(event);
    }

    m_dragging = false;
    if ((event->globalPosition().toPoint() - m_dragStartGlobal).manhattanLength() > kDragClickThreshold) {
        m_movedDuringDrag = true;
    }
    if (m_movedDuringDrag) {
        if (m_movedCallback) {
            m_movedCallback(pos());
        }
    } else if (m_clickedCallback) {
        m_clickedCallback();
    }
    event->accept();
}

void CompanionWindow::applyWindowFlags(bool visible)
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

QColor CompanionWindow::toQColor(const local_jarvis::companion::CompanionColor &color) const
{
    return QColor(color.red, color.green, color.blue);
}
