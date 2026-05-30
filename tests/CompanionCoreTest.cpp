#include "companion/AnimationStateMachine.h"
#include "companion/CompanionAssetRegistry.h"
#include "companion/CompanionManager.h"
#include "companion/CompanionState.h"
#include "companion/CompanionTheme.h"
#include "storage/Storage.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

bool testDefaultState()
{
    using local_jarvis::companion::AnimationState;
    using local_jarvis::companion::CompanionMode;
    using local_jarvis::companion::CompanionState;

    CompanionState state;
    return expect(state.companionVisible, "Companion should default visible.")
        && expect(state.panelVisible, "Panel should default open.")
        && expect(state.panelDefaultOpen, "Panel default-open flag should default true.")
        && expect(state.captionsVisible, "Captions should default visible.")
        && expect(state.currentMode == CompanionMode::Study, "Mode should default to Study.")
        && expect(!state.microphoneEnabled, "Microphone placeholder should default disabled.")
        && expect(state.translationEnabled, "Translation should default enabled.")
        && expect(state.sourceLanguage == "auto", "Source language should default auto.")
        && expect(state.targetLanguage == "en", "Target language should default en.")
        && expect(state.captionFontSize == 24, "Caption font size default changed.")
        && expect(state.captionOpacity == 0.85, "Caption opacity default changed.")
        && expect(state.captionMaxLines == 2, "Caption max lines default changed.")
        && expect(state.captionWidth == 520, "Caption width default changed.")
        && expect(state.captionHeight == 140, "Caption height default changed.")
        && expect(state.captionX == 680 && state.captionY == 520, "Caption position default changed.")
        && expect(state.captionDetached, "Caption should default detached.")
        && expect(!state.captionLocked, "Caption should default unlocked.")
        && expect(state.anchorX == 1200 && state.anchorY == 700, "Anchor default changed.")
        && expect(!state.companionLocked, "Companion should default unlocked.")
        && expect(state.companionScale == 1.0, "Companion scale default changed.")
        && expect(state.themePack == "default", "Theme pack default changed.")
        && expect(state.animationEnabled, "Animation should default enabled.")
        && expect(state.idleMotionEnabled, "Idle motion should default enabled.")
        && expect(state.alwaysOnTop, "Always-on-top should default enabled.")
        && expect(state.currentAnimationState == AnimationState::Idle, "Animation should default idle.");
}

bool testAnimationStateMachine()
{
    using local_jarvis::companion::AnimationState;
    using local_jarvis::companion::AnimationStateMachine;

    AnimationStateMachine machine;
    if (!expect(machine.state() == AnimationState::Idle, "Animation should start idle.")) {
        return false;
    }
    machine.onCompanionClicked();
    if (!expect(machine.state() == AnimationState::Salute, "Click should trigger salute.")) {
        return false;
    }
    machine.onCaptionUpdated();
    if (!expect(machine.state() == AnimationState::TakingNote, "Caption update should trigger taking-note.")) {
        return false;
    }
    machine.onPanelAction();
    if (!expect(machine.state() == AnimationState::Working, "Panel action should trigger working.")) {
        return false;
    }
    machine.onListeningChanged(true);
    if (!expect(machine.state() == AnimationState::Listening, "Listening flag should trigger listening.")) {
        return false;
    }
    machine.onListeningChanged(false);
    machine.onTimeout();
    return expect(machine.state() == AnimationState::Idle, "Timeout should return to idle.");
}

bool testAnimationPriorityAndFallback()
{
    using local_jarvis::companion::AnimationState;
    using local_jarvis::companion::AnimationStateMachine;
    using local_jarvis::companion::CompanionMode;
    using local_jarvis::companion::metadataForAnimation;

    AnimationStateMachine machine;
    machine.onCompanionClicked();
    if (!expect(machine.state() == AnimationState::Salute, "Salute should override idle.")) {
        return false;
    }
    if (!expect(metadataForAnimation(AnimationState::Salute).priority > metadataForAnimation(AnimationState::Idle).priority,
            "Salute priority should be higher than idle.")) {
        return false;
    }

    machine.onTimeout();
    machine.onListeningChanged(true);
    if (!expect(machine.state() == AnimationState::Listening, "Mic placeholder ON should prefer listening.")) {
        return false;
    }
    machine.onPanelAction();
    machine.onTimeout();
    if (!expect(machine.state() == AnimationState::Listening, "Listening should be fallback while mic placeholder is ON.")) {
        return false;
    }
    machine.onListeningChanged(false);
    machine.onCaptionUpdated(CompanionMode::Study);
    if (!expect(machine.state() == AnimationState::TakingNote, "Study dummy caption should prefer taking-note.")) {
        return false;
    }
    machine.onPanelAction();
    if (!expect(machine.state() == AnimationState::Working, "Panel action should trigger working.")) {
        return false;
    }
    machine.onTimeout();
    return expect(machine.state() == AnimationState::Idle, "Working should return to idle after timeout.");
}

bool testVisualProfilesAndAssets()
{
    using local_jarvis::companion::AnimationState;
    using local_jarvis::companion::CompanionMode;
    using local_jarvis::companion::defaultCompanionAssetRegistry;
    using local_jarvis::companion::defaultVisualProfileForMode;
    using local_jarvis::companion::placeholderPathForMode;
    using local_jarvis::companion::supportsAnimation;
    using local_jarvis::companion::supportsMode;

    const auto study = defaultVisualProfileForMode(CompanionMode::Study);
    const auto meeting = defaultVisualProfileForMode(CompanionMode::Meeting);
    const auto interview = defaultVisualProfileForMode(CompanionMode::InterviewPractice);
    const auto review = defaultVisualProfileForMode(CompanionMode::Review);
    const auto registry = defaultCompanionAssetRegistry();

    return expect(study.outfitLabel == "Study Uniform", "Study outfit label changed.")
        && expect(study.accessoryLabel == "Glasses + Notebook", "Study accessory label changed.")
        && expect(study.defaultAnimation == AnimationState::TakingNote, "Study default animation changed.")
        && expect(meeting.outfitLabel == "Smart Blazer", "Meeting outfit label changed.")
        && expect(meeting.accessoryLabel == "Tablet", "Meeting accessory label changed.")
        && expect(meeting.defaultAnimation == AnimationState::Listening, "Meeting default animation changed.")
        && expect(interview.outfitLabel == "Coach Formal", "Interview outfit label changed.")
        && expect(interview.accessoryLabel == "Cue Cards", "Interview accessory label changed.")
        && expect(interview.defaultAnimation == AnimationState::Working, "Interview default animation changed.")
        && expect(review.outfitLabel == "Comfy Reader", "Review outfit label changed.")
        && expect(review.accessoryLabel == "Book Stack", "Review accessory label changed.")
        && expect(review.defaultAnimation == AnimationState::Idle, "Review default animation changed.")
        && expect(supportsMode(registry, CompanionMode::Study), "Asset registry should support Study.")
        && expect(supportsAnimation(registry, AnimationState::Salute), "Asset registry should support Salute.")
        && expect(placeholderPathForMode(registry, CompanionMode::InterviewPractice).value_or("").find("interview_practice.txt") != std::string::npos,
            "Interview placeholder path changed.");
}

bool testSettingsRoundTrip()
{
    using local_jarvis::companion::AnimationState;
    using local_jarvis::companion::CompanionManager;
    using local_jarvis::companion::CompanionMode;
    using local_jarvis::storage::Storage;

    const auto dbPath = std::filesystem::temp_directory_path() / "local-jarvis-companion-test.sqlite";
    std::filesystem::remove(dbPath);

    Storage storage;
    if (!storage.initialize(dbPath)) {
        std::cerr << "Failed to initialize storage: " << storage.lastError() << '\n';
        return false;
    }

    CompanionManager manager(storage);
    if (!expect(manager.loadSettings(), "Companion settings should load.")) {
        return false;
    }
    if (!expect(storage.getSetting("companion.mode").value_or("") == "Study", "Default mode should persist.")) {
        return false;
    }
    if (!expect(manager.state().panelVisible, "Panel should load open by default.")) {
        return false;
    }

    manager.setCompanionVisible(false);
    if (!expect(manager.state().companionVisible, "Companion manager should not hide the robot.")) {
        return false;
    }
    manager.setMode(CompanionMode::Meeting);
    manager.setPanelVisible(false);
    manager.setPanelDefaultOpen(true);
    manager.setCaptionsVisible(false);
    manager.setTranslationEnabled(false);
    manager.setCaptionFontSize(30);
    manager.setCaptionOpacity(0.5);
    manager.setCaptionMaxLines(3);
    manager.setCaptionWidth(640);
    manager.setCaptionHeight(180);
    manager.setCaptionPosition(111, 222);
    manager.setCaptionDetached(true);
    manager.setCaptionLocked(true);
    manager.setAnchorPosition(321, 654);
    manager.setCompanionLocked(true);
    manager.setCompanionScale(1.25);
    manager.setThemePack("default");
    manager.setAnimationEnabled(false);
    manager.setIdleMotionEnabled(false);
    manager.setAlwaysOnTop(false);
    manager.setAnimationState(AnimationState::Working);

    CompanionManager reloaded(storage);
    if (!expect(reloaded.loadSettings(), "Reloaded companion settings should load.")) {
        return false;
    }
    const auto &state = reloaded.state();
    const bool ok = expect(state.companionVisible, "Companion visibility should stay true after reload.")
        && expect(state.panelVisible, "Panel should reopen on load when default-open is enabled.")
        && expect(state.panelDefaultOpen, "Panel default-open flag did not persist.")
        && expect(state.currentMode == CompanionMode::Meeting, "Mode did not persist.")
        && expect(!state.captionsVisible, "Captions visibility did not persist.")
        && expect(!state.translationEnabled, "Translation flag did not persist.")
        && expect(state.captionFontSize == 30, "Caption font size did not persist.")
        && expect(state.captionOpacity == 0.5, "Caption opacity did not persist.")
        && expect(state.captionMaxLines == 3, "Caption max lines did not persist.")
        && expect(state.captionWidth == 640, "Caption width did not persist.")
        && expect(state.captionHeight == 180, "Caption height did not persist.")
        && expect(state.captionX == 111 && state.captionY == 222, "Caption position did not persist.")
        && expect(state.captionDetached, "Caption detached state did not persist.")
        && expect(state.captionLocked, "Caption locked state did not persist.")
        && expect(state.anchorX == 321 && state.anchorY == 654, "Anchor position did not persist.")
        && expect(!state.companionLocked, "Robot should reload unlocked for free placement.")
        && expect(state.companionScale == 1.25, "Companion scale did not persist.")
        && expect(state.themePack == "default", "Theme pack did not persist.")
        && expect(!state.animationEnabled, "Animation enabled flag did not persist.")
        && expect(!state.idleMotionEnabled, "Idle motion flag did not persist.")
        && expect(!state.alwaysOnTop, "Always-on-top flag did not persist.")
        && expect(state.currentAnimationState == AnimationState::Idle, "Animation state should reload idle.");

    storage.close();
    std::filesystem::remove(dbPath);
    return ok;
}

} // namespace

int main()
{
    const bool ok = testDefaultState()
        && testAnimationStateMachine()
        && testAnimationPriorityAndFallback()
        && testVisualProfilesAndAssets()
        && testSettingsRoundTrip();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
