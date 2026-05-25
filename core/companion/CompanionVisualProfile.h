#pragma once

#include "CompanionTypes.h"

#include <string>

namespace local_jarvis::companion {

struct CompanionColor {
    int red = 68;
    int green = 166;
    int blue = 120;
};

struct CompanionVisualProfile {
    CompanionMode mode = CompanionMode::Study;
    std::string displayName = "Study";
    CompanionColor primaryColor {};
    CompanionColor accentColor { 134, 219, 170 };
    std::string outfitLabel = "Study Uniform";
    std::string accessoryLabel = "Glasses + Notebook";
    AnimationState defaultAnimation = AnimationState::TakingNote;
    std::string captionBubbleStyle = "note-card";
    std::string panelAccentStyle = "study";
};

[[nodiscard]] CompanionColor colorFromHex(const std::string &hex, CompanionColor fallback = {});
[[nodiscard]] std::string colorToHex(const CompanionColor &color);

} // namespace local_jarvis::companion
