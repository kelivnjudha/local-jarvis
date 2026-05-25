#pragma once

#include <string>

namespace local_jarvis::companion {

enum class CompanionMode {
    Study,
    Meeting,
    InterviewPractice,
    Review
};

enum class AnimationState {
    Idle,
    Listening,
    Working,
    TakingNote,
    Walking,
    Salute
};

[[nodiscard]] std::string toString(CompanionMode mode);
[[nodiscard]] CompanionMode companionModeFromString(const std::string &value);
[[nodiscard]] std::string toString(AnimationState state);
[[nodiscard]] AnimationState animationStateFromString(const std::string &value);

} // namespace local_jarvis::companion
