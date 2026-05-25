#pragma once

#include "companion/CompanionManager.h"

#include <QColor>
#include <QPoint>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <vector>

class CaptionBubbleWindow final : public QWidget {
public:
    explicit CaptionBubbleWindow(local_jarvis::companion::CompanionManager &companionManager, QWidget *parent = nullptr);

    void applyState();
    void setAnchorPosition(const QPoint &companionTopLeft);
    void setCaptionUpdatedCallback(std::function<void()> callback);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateDummyCaption();
    void applyWindowFlags(bool visible);
    [[nodiscard]] QColor toQColor(const local_jarvis::companion::CompanionColor &color) const;
    [[nodiscard]] QString currentModeCaption() const;

    local_jarvis::companion::CompanionManager &m_companionManager;
    QTimer m_captionTimer;
    QString m_captionText = "Listening ready...";
    int m_captionIndex = 0;
    QPoint m_companionTopLeft;
    std::function<void()> m_captionUpdatedCallback;
};
