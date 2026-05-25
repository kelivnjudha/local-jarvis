#include "AnimationStateMachine.h"

namespace local_jarvis::companion {

AnimationState AnimationStateMachine::state() const
{
    return m_state;
}

void AnimationStateMachine::reset()
{
    m_state = AnimationState::Idle;
}

void AnimationStateMachine::setState(AnimationState state)
{
    m_state = state;
}

void AnimationStateMachine::onCompanionClicked()
{
    m_state = AnimationState::Salute;
}

void AnimationStateMachine::onPanelAction()
{
    m_state = AnimationState::Working;
}

void AnimationStateMachine::onCaptionUpdated()
{
    m_state = AnimationState::TakingNote;
}

void AnimationStateMachine::onListeningChanged(bool listening)
{
    m_state = listening ? AnimationState::Listening : AnimationState::Idle;
}

void AnimationStateMachine::onTimeout()
{
    m_state = AnimationState::Idle;
}

} // namespace local_jarvis::companion
