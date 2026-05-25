#pragma once

#include "CompanionVisualProfile.h"

#include <string>
#include <vector>

namespace local_jarvis::companion {

struct CompanionThemePack {
    std::string themePackId = "default";
    std::string displayName = "Default Companion";
    std::vector<CompanionVisualProfile> profiles;
};

[[nodiscard]] CompanionThemePack defaultCompanionTheme();
[[nodiscard]] CompanionVisualProfile defaultVisualProfileForMode(CompanionMode mode);
[[nodiscard]] CompanionVisualProfile visualProfileForMode(const CompanionThemePack &theme, CompanionMode mode);

} // namespace local_jarvis::companion
