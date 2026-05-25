#pragma once

#include "CompanionTypes.h"

namespace local_jarvis::companion {

struct AnimationStateMetadata {
    AnimationState state = AnimationState::Idle;
    int transitionDurationMs = 240;
    const char *displayLabel = "Idle";
    int priority = 0;
};

[[nodiscard]] AnimationStateMetadata metadataForAnimation(AnimationState state);
[[nodiscard]] const char *displayLabelForAnimation(AnimationState state);

class AnimationStateMachine {
public:
    [[nodiscard]] AnimationState state() const;
    [[nodiscard]] bool animationEnabled() const;
    [[nodiscard]] bool idleMotionEnabled() const;

    void reset();
    void configure(bool animationEnabled, bool idleMotionEnabled);
    void setState(AnimationState state);
    void onCompanionClicked();
    void onPanelAction();
    void onCaptionUpdated(CompanionMode mode);
    void onCaptionUpdated();
    void onListeningChanged(bool listening);
    void onTimeout();

private:
    void applyRequestedState(AnimationState state);
    void fallbackAfterTransient();

    AnimationState m_state = AnimationState::Idle;
    bool m_animationEnabled = true;
    bool m_idleMotionEnabled = true;
    bool m_microphonePlaceholderEnabled = false;
    int m_idleTimeoutCount = 0;
};

} // namespace local_jarvis::companion
