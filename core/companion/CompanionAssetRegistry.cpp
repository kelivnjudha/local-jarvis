#include "CompanionAssetRegistry.h"

#include <algorithm>

namespace local_jarvis::companion {
namespace {

std::string placeholderFileName(CompanionMode mode)
{
    switch (mode) {
    case CompanionMode::Study:
        return "study.txt";
    case CompanionMode::Meeting:
        return "meeting.txt";
    case CompanionMode::InterviewPractice:
        return "interview_practice.txt";
    case CompanionMode::Review:
        return "review.txt";
    }
    return "study.txt";
}

} // namespace

CompanionAssetRegistry defaultCompanionAssetRegistry()
{
    return CompanionAssetRegistry {
        .characterPackId = "default",
        .characterName = "Local Jarvis Placeholder",
        .assetType = "placeholder",
        .rootPath = "assets/companion/default",
        .supportedModes = {
            CompanionMode::Study,
            CompanionMode::Meeting,
            CompanionMode::InterviewPractice,
            CompanionMode::Review
        },
        .supportedAnimations = {
            AnimationState::Idle,
            AnimationState::Listening,
            AnimationState::Working,
            AnimationState::TakingNote,
            AnimationState::Walking,
            AnimationState::Salute
        }
    };
}

bool supportsMode(const CompanionAssetRegistry &registry, CompanionMode mode)
{
    return std::find(registry.supportedModes.begin(), registry.supportedModes.end(), mode)
        != registry.supportedModes.end();
}

bool supportsAnimation(const CompanionAssetRegistry &registry, AnimationState animation)
{
    return std::find(registry.supportedAnimations.begin(), registry.supportedAnimations.end(), animation)
        != registry.supportedAnimations.end();
}

std::string manifestPath(const CompanionAssetRegistry &registry)
{
    return registry.rootPath + "/manifest.json";
}

std::optional<std::string> placeholderPathForMode(const CompanionAssetRegistry &registry, CompanionMode mode)
{
    if (!supportsMode(registry, mode)) {
        return std::nullopt;
    }
    return registry.rootPath + "/placeholders/" + placeholderFileName(mode);
}

} // namespace local_jarvis::companion
