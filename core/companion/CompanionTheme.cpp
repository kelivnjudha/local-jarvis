#include "CompanionTheme.h"

namespace local_jarvis::companion {

CompanionThemePack defaultCompanionTheme()
{
    return CompanionThemePack {
        .themePackId = "default",
        .displayName = "Default Companion",
        .profiles = {
            CompanionVisualProfile {
                .mode = CompanionMode::Study,
                .displayName = "Study",
                .primaryColor = { 68, 166, 120 },
                .accentColor = { 139, 222, 171 },
                .outfitLabel = "Study Uniform",
                .accessoryLabel = "Glasses + Notebook",
                .defaultAnimation = AnimationState::TakingNote,
                .captionBubbleStyle = "note-card",
                .panelAccentStyle = "study"
            },
            CompanionVisualProfile {
                .mode = CompanionMode::Meeting,
                .displayName = "Meeting",
                .primaryColor = { 74, 128, 214 },
                .accentColor = { 156, 192, 244 },
                .outfitLabel = "Smart Blazer",
                .accessoryLabel = "Tablet",
                .defaultAnimation = AnimationState::Listening,
                .captionBubbleStyle = "meeting-caption",
                .panelAccentStyle = "meeting"
            },
            CompanionVisualProfile {
                .mode = CompanionMode::InterviewPractice,
                .displayName = "Interview Practice",
                .primaryColor = { 206, 132, 65 },
                .accentColor = { 239, 184, 128 },
                .outfitLabel = "Coach Formal",
                .accessoryLabel = "Cue Cards",
                .defaultAnimation = AnimationState::Working,
                .captionBubbleStyle = "coaching-prompt",
                .panelAccentStyle = "coach"
            },
            CompanionVisualProfile {
                .mode = CompanionMode::Review,
                .displayName = "Review",
                .primaryColor = { 144, 114, 210 },
                .accentColor = { 196, 178, 238 },
                .outfitLabel = "Comfy Reader",
                .accessoryLabel = "Book Stack",
                .defaultAnimation = AnimationState::Idle,
                .captionBubbleStyle = "reading-review",
                .panelAccentStyle = "review"
            }
        }
    };
}

CompanionVisualProfile defaultVisualProfileForMode(CompanionMode mode)
{
    return visualProfileForMode(defaultCompanionTheme(), mode);
}

CompanionVisualProfile visualProfileForMode(const CompanionThemePack &theme, CompanionMode mode)
{
    for (const auto &profile : theme.profiles) {
        if (profile.mode == mode) {
            return profile;
        }
    }
    return CompanionVisualProfile {};
}

} // namespace local_jarvis::companion
