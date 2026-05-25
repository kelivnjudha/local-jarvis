#pragma once

#include "companion/CompanionManager.h"

#include <QPoint>
#include <QTimer>
#include <QWidget>

#include <functional>

class CompanionWindow final : public QWidget {
public:
    explicit CompanionWindow(local_jarvis::companion::CompanionManager &companionManager, QWidget *parent = nullptr);

    void applyState();
    void setClickedCallback(std::function<void()> callback);
    void setMovedCallback(std::function<void(const QPoint &)> callback);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QColor accentColor() const;
    QString modeLabel() const;

    local_jarvis::companion::CompanionManager &m_companionManager;
    QTimer m_animationTimer;
    int m_frame = 0;
    bool m_dragging = false;
    bool m_movedDuringDrag = false;
    QPoint m_dragStartGlobal;
    QPoint m_dragWindowOffset;
    std::function<void()> m_clickedCallback;
    std::function<void(const QPoint &)> m_movedCallback;
};
