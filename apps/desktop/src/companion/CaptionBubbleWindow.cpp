#include "companion/CaptionBubbleWindow.h"

#include <QPainter>
#include <QTextOption>
#include <QtGlobal>

CaptionBubbleWindow::CaptionBubbleWindow(
    local_jarvis::companion::CompanionManager &companionManager,
    QWidget *parent)
    : QWidget(parent)
    , m_companionManager(companionManager)
{
    setWindowTitle("Local Jarvis Captions");
    setAccessibleName("Local Jarvis Caption Bubble");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(&m_captionTimer, &QTimer::timeout, this, [this]() {
        updateDummyCaption();
    });
    m_captionTimer.start(3200);
}

void CaptionBubbleWindow::applyState()
{
    const auto &state = m_companionManager.state();
    resize(state.captionWidth, qMax(80, state.captionFontSize * (state.captionMaxLines + 2)));
    setWindowOpacity(state.captionOpacity);
    applyWindowFlags(state.companionVisible && state.captionsVisible);
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

    painter.setPen(foreground);
    QFont font = painter.font();
    font.setPointSize(m_companionManager.state().captionFontSize);
    font.setBold(true);
    painter.setFont(font);

    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    painter.drawText(rect().adjusted(20, 18, -20, -14), m_captionText, option);
}

void CaptionBubbleWindow::updateDummyCaption()
{
    const std::vector<QString> captions {
        "Listening ready...",
        "Taking notes locally...",
        currentModeCaption(),
        QString::fromUtf8("Translation: Auto \xE2\x86\x92 English")
    };

    m_captionText = captions[static_cast<std::size_t>(m_captionIndex) % captions.size()];
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

QColor CaptionBubbleWindow::toQColor(const local_jarvis::companion::CompanionColor &color) const
{
    return QColor(color.red, color.green, color.blue);
}

QString CaptionBubbleWindow::currentModeCaption() const
{
    using local_jarvis::companion::CompanionMode;
    switch (m_companionManager.state().currentMode) {
    case CompanionMode::Study:
        return "Study mode active.";
    case CompanionMode::Meeting:
        return "Meeting mode active.";
    case CompanionMode::InterviewPractice:
        return "Interview practice active.";
    case CompanionMode::Review:
        return "Review mode active.";
    }
    return "Study mode active.";
}
