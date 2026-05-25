#pragma once

#include "caption/CaptionManager.h"
#include "companion/CompanionManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QWidget>

#include <functional>

class AssistantPanelWindow final : public QWidget {
public:
    explicit AssistantPanelWindow(
        local_jarvis::companion::CompanionManager &companionManager,
        local_jarvis::caption::CaptionManager &captionManager,
        QWidget *parent = nullptr);

    void applyState();
    void setAnchorPosition(const QPoint &companionTopLeft);
    void setCallbacks(
        std::function<void()> changedCallback,
        std::function<void()> settingsCallback,
        std::function<void()> openFullAppCallback,
        std::function<void()> closeCallback,
        std::function<void()> panelActionCallback);

private:
    void buildUi();
    void connectSignals();
    void emitPanelAction();
    void applyWindowFlags(bool visible);
    [[nodiscard]] local_jarvis::companion::CompanionMode selectedMode() const;
    [[nodiscard]] local_jarvis::caption::CaptionMode selectedCaptionMode() const;
    [[nodiscard]] int captionModeIndex(local_jarvis::caption::CaptionMode mode) const;

    local_jarvis::companion::CompanionManager &m_companionManager;
    local_jarvis::caption::CaptionManager &m_captionManager;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_captionModeCombo = nullptr;
    QCheckBox *m_captionsCheck = nullptr;
    QCheckBox *m_showSpeakerCheck = nullptr;
    QCheckBox *m_translationCheck = nullptr;
    QCheckBox *m_microphoneCheck = nullptr;
    QLabel *m_languageLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QPushButton *m_openFullAppButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    std::function<void()> m_changedCallback;
    std::function<void()> m_settingsCallback;
    std::function<void()> m_openFullAppCallback;
    std::function<void()> m_closeCallback;
    std::function<void()> m_panelActionCallback;
};
