#include "companion/AnimationStateMachine.h"
#include "companion/CompanionManager.h"
#include "companion/CompanionState.h"
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
        && expect(!state.panelVisible, "Panel should default hidden.")
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
        && expect(state.anchorX == 1200 && state.anchorY == 700, "Anchor default changed.")
        && expect(!state.companionLocked, "Companion should default unlocked.")
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
    machine.onTimeout();
    return expect(machine.state() == AnimationState::Idle, "Timeout should return to idle.");
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

    manager.setMode(CompanionMode::Meeting);
    manager.setCaptionsVisible(false);
    manager.setTranslationEnabled(false);
    manager.setCaptionFontSize(30);
    manager.setCaptionOpacity(0.5);
    manager.setCaptionMaxLines(3);
    manager.setCaptionWidth(640);
    manager.setAnchorPosition(321, 654);
    manager.setCompanionLocked(true);
    manager.setAnimationState(AnimationState::Working);

    CompanionManager reloaded(storage);
    if (!expect(reloaded.loadSettings(), "Reloaded companion settings should load.")) {
        return false;
    }
    const auto &state = reloaded.state();
    const bool ok = expect(state.currentMode == CompanionMode::Meeting, "Mode did not persist.")
        && expect(!state.captionsVisible, "Captions visibility did not persist.")
        && expect(!state.translationEnabled, "Translation flag did not persist.")
        && expect(state.captionFontSize == 30, "Caption font size did not persist.")
        && expect(state.captionOpacity == 0.5, "Caption opacity did not persist.")
        && expect(state.captionMaxLines == 3, "Caption max lines did not persist.")
        && expect(state.captionWidth == 640, "Caption width did not persist.")
        && expect(state.anchorX == 321 && state.anchorY == 654, "Anchor position did not persist.")
        && expect(state.companionLocked, "Locked state did not persist.")
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
        && testSettingsRoundTrip();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
