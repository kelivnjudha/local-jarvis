#pragma once

#include "caption/CaptionManager.h"
#include "caption/DummyCaptionSource.h"
#include "companion/CompanionManager.h"

#include <QColor>
#include <QLabel>
#include <QPoint>
#include <QTimer>
#include <QWidget>

#include <functional>

class CaptionBubbleWindow final : public QWidget {
public:
    explicit CaptionBubbleWindow(
        local_jarvis::companion::CompanionManager &companionManager,
        local_jarvis::caption::CaptionManager &captionManager,
        local_jarvis::caption::DummyCaptionSource &dummyCaptionSource,
        QWidget *parent = nullptr);

    void applyState();
    void setAnchorPosition(const QPoint &companionTopLeft);
    void setCaptionUpdatedCallback(std::function<void()> callback);
    void setMicrophonePlaceholderText(const QString &text);
    void setDummyCaptionsEnabled(bool enabled);
    void refreshCaptionText();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateDummyCaption();
    void applyWindowFlags(bool visible);
    void updateCaptionLabelStyle();
    [[nodiscard]] QColor toQColor(const local_jarvis::companion::CompanionColor &color) const;

    local_jarvis::companion::CompanionManager &m_companionManager;
    local_jarvis::caption::CaptionManager &m_captionManager;
    local_jarvis::caption::DummyCaptionSource &m_dummyCaptionSource;
    QTimer m_captionTimer;
    QLabel *m_captionLabel = nullptr;
    QString m_captionText = "Listening ready...";
    QString m_microphonePlaceholderText;
    int m_captionIndex = 0;
    bool m_dummyCaptionsEnabled = true;
    QPoint m_companionTopLeft;
    std::function<void()> m_captionUpdatedCallback;
};
