#include "companion/CompanionWindow.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace {

constexpr int kWindowWidth = 132;
constexpr int kWindowHeight = 150;

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
    setFixedSize(kWindowWidth, kWindowHeight);
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
    move(state.anchorX, state.anchorY);
    setVisible(state.companionVisible);
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
    const auto animation = state.currentAnimationState;
    const int bob = static_cast<int>(qSin(m_frame / 8.0) * 4.0);
    const int yOffset = animation == local_jarvis::companion::AnimationState::Walking ? bob : 0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor accent = accentColor();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(18, 22, 28, 225));
    painter.drawRoundedRect(rect().adjusted(8, 12, -8, -8), 18, 18);

    painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 60));
    painter.drawEllipse(QPoint(width() / 2, 78 + yOffset), 46, 54);

    painter.setBrush(accent);
    painter.drawEllipse(QPoint(width() / 2, 54 + yOffset), 30, 30);

    painter.setBrush(QColor(245, 248, 252));
    painter.drawEllipse(QPoint(width() / 2 - 10, 50 + yOffset), 5, 7);
    painter.drawEllipse(QPoint(width() / 2 + 10, 50 + yOffset), 5, 7);

    painter.setPen(QPen(QColor(245, 248, 252), 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRect(width() / 2 - 12, 58 + yOffset, 24, 16), 200 * 16, 140 * 16);

    painter.setPen(QPen(accent.lighter(130), 8, Qt::SolidLine, Qt::RoundCap));
    const QPoint leftShoulder(width() / 2 - 28, 84 + yOffset);
    const QPoint rightShoulder(width() / 2 + 28, 84 + yOffset);
    painter.drawLine(leftShoulder, QPoint(width() / 2 - 46, 112 + yOffset));
    if (animation == local_jarvis::companion::AnimationState::Salute) {
        painter.drawLine(rightShoulder, QPoint(width() / 2 + 36, 42 + yOffset));
    } else {
        painter.drawLine(rightShoulder, QPoint(width() / 2 + 46, 112 + yOffset));
    }

    painter.setPen(QPen(accent.lighter(140), 7, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPoint(width() / 2 - 16, 118 + yOffset), QPoint(width() / 2 - 22, 136));
    painter.drawLine(QPoint(width() / 2 + 16, 118 + yOffset), QPoint(width() / 2 + 22, 136));

    painter.setPen(QColor(235, 238, 244));
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(9);
    painter.setFont(font);
    painter.drawText(QRect(14, 118, width() - 28, 22), Qt::AlignCenter, modeLabel());

    font.setPointSize(8);
    font.setBold(false);
    painter.setFont(font);
    painter.setPen(QColor(185, 194, 206));
    painter.drawText(QRect(14, 132, width() - 28, 14), Qt::AlignCenter, QString::fromStdString(toString(state.currentAnimationState)));
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
    if ((globalPosition - m_dragStartGlobal).manhattanLength() > 4) {
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
    if (m_movedDuringDrag) {
        if (m_movedCallback) {
            m_movedCallback(pos());
        }
    } else if (m_clickedCallback) {
        m_clickedCallback();
    }
    event->accept();
}

QColor CompanionWindow::accentColor() const
{
    using local_jarvis::companion::CompanionMode;
    switch (m_companionManager.state().currentMode) {
    case CompanionMode::Study:
        return QColor(68, 166, 120);
    case CompanionMode::Meeting:
        return QColor(74, 128, 214);
    case CompanionMode::InterviewPractice:
        return QColor(206, 132, 65);
    case CompanionMode::Review:
        return QColor(144, 114, 210);
    }
    return QColor(68, 166, 120);
}

QString CompanionWindow::modeLabel() const
{
    return QString::fromStdString(toString(m_companionManager.state().currentMode));
}
