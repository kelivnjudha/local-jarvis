#pragma once

#include "CompanionTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace local_jarvis::companion {

struct CompanionAssetRegistry {
    std::string characterPackId = "default";
    std::string characterName = "Local Jarvis Placeholder";
    std::string assetType = "placeholder";
    std::string rootPath = "assets/companion/default";
    std::vector<CompanionMode> supportedModes;
    std::vector<AnimationState> supportedAnimations;
};

[[nodiscard]] CompanionAssetRegistry defaultCompanionAssetRegistry();
[[nodiscard]] bool supportsMode(const CompanionAssetRegistry &registry, CompanionMode mode);
[[nodiscard]] bool supportsAnimation(const CompanionAssetRegistry &registry, AnimationState animation);
[[nodiscard]] std::string manifestPath(const CompanionAssetRegistry &registry);
[[nodiscard]] std::optional<std::string> placeholderPathForMode(const CompanionAssetRegistry &registry, CompanionMode mode);

} // namespace local_jarvis::companion
