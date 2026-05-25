#include "AnimationStateMachine.h"

namespace local_jarvis::companion {

AnimationStateMetadata metadataForAnimation(AnimationState state)
{
    switch (state) {
    case AnimationState::Idle:
        return { state, 180, "Idle", 0 };
    case AnimationState::Walking:
        return { state, 260, "Walking", 1 };
    case AnimationState::Listening:
        return { state, 220, "Listening", 2 };
    case AnimationState::TakingNote:
        return { state, 220, "Taking Note", 3 };
    case AnimationState::Working:
        return { state, 240, "Working", 4 };
    case AnimationState::Salute:
        return { state, 160, "Salute", 5 };
    }
    return { AnimationState::Idle, 180, "Idle", 0 };
}

const char *displayLabelForAnimation(AnimationState state)
{
    return metadataForAnimation(state).displayLabel;
}

AnimationState AnimationStateMachine::state() const
{
    return m_state;
}

bool AnimationStateMachine::animationEnabled() const
{
    return m_animationEnabled;
}

bool AnimationStateMachine::idleMotionEnabled() const
{
    return m_idleMotionEnabled;
}

void AnimationStateMachine::reset()
{
    m_state = AnimationState::Idle;
    m_idleTimeoutCount = 0;
}

void AnimationStateMachine::configure(bool animationEnabled, bool idleMotionEnabled)
{
    m_animationEnabled = animationEnabled;
    m_idleMotionEnabled = idleMotionEnabled;
    if (!m_animationEnabled) {
        reset();
    }
}

void AnimationStateMachine::setState(AnimationState state)
{
    applyRequestedState(state);
}

void AnimationStateMachine::onCompanionClicked()
{
    applyRequestedState(AnimationState::Salute);
}

void AnimationStateMachine::onPanelAction()
{
    applyRequestedState(AnimationState::Working);
}

void AnimationStateMachine::onCaptionUpdated(CompanionMode mode)
{
    if (mode == CompanionMode::Study) {
        applyRequestedState(AnimationState::TakingNote);
    } else {
        fallbackAfterTransient();
    }
}

void AnimationStateMachine::onCaptionUpdated()
{
    onCaptionUpdated(CompanionMode::Study);
}

void AnimationStateMachine::onListeningChanged(bool listening)
{
    m_microphonePlaceholderEnabled = listening;
    if (listening) {
        applyRequestedState(AnimationState::Listening);
        return;
    }
    fallbackAfterTransient();
}

void AnimationStateMachine::onTimeout()
{
    fallbackAfterTransient();
}

void AnimationStateMachine::applyRequestedState(AnimationState state)
{
    if (!m_animationEnabled) {
        m_state = AnimationState::Idle;
        return;
    }

    m_state = state;
    if (state != AnimationState::Walking && state != AnimationState::Idle) {
        m_idleTimeoutCount = 0;
    }
}

void AnimationStateMachine::fallbackAfterTransient()
{
    if (!m_animationEnabled) {
        m_state = AnimationState::Idle;
        return;
    }

    if (m_microphonePlaceholderEnabled) {
        m_state = AnimationState::Listening;
        return;
    }

    if (m_idleMotionEnabled && m_state == AnimationState::Idle) {
        ++m_idleTimeoutCount;
        if (m_idleTimeoutCount % 4 == 0) {
            m_state = AnimationState::Walking;
            return;
        }
    }

    m_state = AnimationState::Idle;
}

} // namespace local_jarvis::companion
