#pragma once

#include "CompanionTypes.h"

#include <string>

namespace local_jarvis::companion {

struct CompanionState {
    bool companionVisible = true;
    bool panelVisible = true;
    bool panelDefaultOpen = true;
    bool captionsVisible = true;
    CompanionMode currentMode = CompanionMode::Study;
    bool microphoneEnabled = false;
    bool translationEnabled = true;
    std::string sourceLanguage = "auto";
    std::string targetLanguage = "en";
    int captionFontSize = 24;
    double captionOpacity = 0.85;
    int captionMaxLines = 2;
    int captionWidth = 520;
    int captionHeight = 140;
    int captionX = 680;
    int captionY = 520;
    bool captionDetached = true;
    bool captionLocked = false;
    int anchorX = 1200;
    int anchorY = 700;
    bool companionLocked = false;
    double companionScale = 1.0;
    std::string themePack = "default";
    bool animationEnabled = true;
    bool idleMotionEnabled = true;
    bool alwaysOnTop = true;
    AnimationState currentAnimationState = AnimationState::Idle;
};

} // namespace local_jarvis::companion
