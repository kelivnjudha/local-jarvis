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
    setVisible(state.companionVisible && state.captionsVisible);
    update();
}

void CaptionBubbleWindow::setAnchorPosition(const QPoint &companionTopLeft)
{
    m_companionTopLeft = companionTopLeft;
    move(m_companionTopLeft + QPoint(-qMax(0, width() - 132), -height() - 12));
}

void CaptionBubbleWindow::setCaptionUpdatedCallback(std::function<void()> callback)
{
    m_captionUpdatedCallback = std::move(callback);
}

void CaptionBubbleWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 24, 30, 235));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 14, 14);

    painter.setPen(QColor(238, 242, 248));
    QFont font = painter.font();
    font.setPointSize(m_companionManager.state().captionFontSize);
    font.setBold(true);
    painter.setFont(font);

    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    painter.drawText(rect().adjusted(18, 12, -18, -12), m_captionText, option);
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
