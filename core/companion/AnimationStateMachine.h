#pragma once

#include "CompanionTypes.h"

namespace local_jarvis::companion {

class AnimationStateMachine {
public:
    [[nodiscard]] AnimationState state() const;

    void reset();
    void setState(AnimationState state);
    void onCompanionClicked();
    void onPanelAction();
    void onCaptionUpdated();
    void onListeningChanged(bool listening);
    void onTimeout();

private:
    AnimationState m_state = AnimationState::Idle;
};

} // namespace local_jarvis::companion
