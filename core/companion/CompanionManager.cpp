#include "CompanionManager.h"

#include "storage/Storage.h"

#include <algorithm>
#include <string>

namespace local_jarvis::companion {
namespace {

constexpr const char *kCompanionVisible = "companion.visible";
constexpr const char *kPanelVisible = "companion.panel.visible";
constexpr const char *kPanelDefaultOpen = "companion.panel.default_open";
constexpr const char *kCaptionsVisible = "companion.captions.visible";
constexpr const char *kMode = "companion.mode";
constexpr const char *kAnchorX = "companion.anchor_x";
constexpr const char *kAnchorY = "companion.anchor_y";
constexpr const char *kLocked = "companion.locked";
constexpr const char *kCaptionFontSize = "companion.caption.font_size";
constexpr const char *kCaptionOpacity = "companion.caption.opacity";
constexpr const char *kCaptionMaxLines = "companion.caption.max_lines";
constexpr const char *kCaptionWidthLegacy = "companion.caption.width";
constexpr const char *kCaptionWidth = "caption.width";
constexpr const char *kCaptionHeight = "caption.height";
constexpr const char *kCaptionX = "caption.x";
constexpr const char *kCaptionY = "caption.y";
constexpr const char *kCaptionDetached = "caption.detached";
constexpr const char *kCaptionLocked = "caption.locked";
constexpr const char *kTranslationEnabled = "companion.translation.enabled";
constexpr const char *kSourceLanguage = "companion.source_language";
constexpr const char *kTargetLanguage = "companion.target_language";
constexpr const char *kScale = "companion.scale";
constexpr const char *kThemePack = "companion.theme_pack";
constexpr const char *kAnimationEnabled = "companion.animation_enabled";
constexpr const char *kIdleMotionEnabled = "companion.idle_motion_enabled";
constexpr const char *kAlwaysOnTop = "companion.always_on_top";

std::string boolText(bool value)
{
    return value ? "true" : "false";
}

bool parseBool(const std::string &value, bool fallback)
{
    if (value == "true" || value == "1" || value == "yes") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no") {
        return false;
    }
    return fallback;
}

int clampInt(int value, int minimum, int maximum)
{
    return std::clamp(value, minimum, maximum);
}

double clampDouble(double value, double minimum, double maximum)
{
    return std::clamp(value, minimum, maximum);
}

} // namespace

std::string toString(CompanionMode mode)
{
    switch (mode) {
    case CompanionMode::Study:
        return "Study";
    case CompanionMode::Meeting:
        return "Meeting";
    case CompanionMode::InterviewPractice:
        return "InterviewPractice";
    case CompanionMode::Review:
        return "Review";
    }
    return "Study";
}

CompanionMode companionModeFromString(const std::string &value)
{
    if (value == "Meeting") {
        return CompanionMode::Meeting;
    }
    if (value == "InterviewPractice") {
        return CompanionMode::InterviewPractice;
    }
    if (value == "Review") {
        return CompanionMode::Review;
    }
    return CompanionMode::Study;
}

std::string toString(AnimationState state)
{
    switch (state) {
    case AnimationState::Idle:
        return "Idle";
    case AnimationState::Listening:
        return "Listening";
    case AnimationState::Working:
        return "Working";
    case AnimationState::TakingNote:
        return "TakingNote";
    case AnimationState::Walking:
        return "Walking";
    case AnimationState::Salute:
        return "Salute";
    }
    return "Idle";
}

AnimationState animationStateFromString(const std::string &value)
{
    if (value == "Listening") {
        return AnimationState::Listening;
    }
    if (value == "Working") {
        return AnimationState::Working;
    }
    if (value == "TakingNote") {
        return AnimationState::TakingNote;
    }
    if (value == "Walking") {
        return AnimationState::Walking;
    }
    if (value == "Salute") {
        return AnimationState::Salute;
    }
    return AnimationState::Idle;
}

CompanionManager::CompanionManager(storage::Storage &storage)
    : m_storage(storage)
{
}

bool CompanionManager::loadSettings()
{
    if (!m_storage.isOpen()) {
        return false;
    }

    CompanionState defaults;
    (void)settingBool(kCompanionVisible, defaults.companionVisible);
    m_state.companionVisible = true;
    saveBool(kCompanionVisible, true);
    m_state.panelDefaultOpen = settingBool(kPanelDefaultOpen, defaults.panelDefaultOpen);
    m_state.panelVisible = m_state.panelDefaultOpen
        ? true
        : settingBool(kPanelVisible, defaults.panelVisible);
    m_state.captionsVisible = settingBool(kCaptionsVisible, defaults.captionsVisible);
    m_state.currentMode = companionModeFromString(settingString(kMode, toString(defaults.currentMode)));
    m_state.anchorX = settingInt(kAnchorX, defaults.anchorX);
    m_state.anchorY = settingInt(kAnchorY, defaults.anchorY);
    (void)settingBool(kLocked, defaults.companionLocked);
    m_state.companionLocked = false;
    saveBool(kLocked, false);
    m_state.captionFontSize = clampInt(settingInt(kCaptionFontSize, defaults.captionFontSize), 12, 48);
    m_state.captionOpacity = clampDouble(settingDouble(kCaptionOpacity, defaults.captionOpacity), 0.2, 1.0);
    m_state.captionMaxLines = clampInt(settingInt(kCaptionMaxLines, defaults.captionMaxLines), 1, 5);
    auto captionWidthSetting = m_storage.getSetting(kCaptionWidth);
    if (!captionWidthSetting.has_value()) {
        captionWidthSetting = m_storage.getSetting(kCaptionWidthLegacy);
    }
    if (captionWidthSetting.has_value()) {
        try {
            m_state.captionWidth = clampInt(std::stoi(*captionWidthSetting), 260, 1200);
        } catch (...) {
            m_state.captionWidth = defaults.captionWidth;
        }
        saveInt(kCaptionWidth, m_state.captionWidth);
        saveInt(kCaptionWidthLegacy, m_state.captionWidth);
    } else {
        m_state.captionWidth = clampInt(settingInt(kCaptionWidth, defaults.captionWidth), 260, 1200);
        saveInt(kCaptionWidthLegacy, m_state.captionWidth);
    }
    m_state.captionHeight = clampInt(settingInt(kCaptionHeight, defaults.captionHeight), 80, 500);
    m_state.captionX = settingInt(kCaptionX, defaults.captionX);
    m_state.captionY = settingInt(kCaptionY, defaults.captionY);
    m_state.captionDetached = settingBool(kCaptionDetached, defaults.captionDetached);
    m_state.captionLocked = settingBool(kCaptionLocked, defaults.captionLocked);
    m_state.translationEnabled = settingBool(kTranslationEnabled, defaults.translationEnabled);
    m_state.sourceLanguage = settingString(kSourceLanguage, defaults.sourceLanguage);
    m_state.targetLanguage = settingString(kTargetLanguage, defaults.targetLanguage);
    m_state.companionScale = clampDouble(settingDouble(kScale, defaults.companionScale), 0.65, 1.6);
    m_state.themePack = settingString(kThemePack, defaults.themePack);
    if (m_state.themePack.empty()) {
        m_state.themePack = defaults.themePack;
    }
    m_state.animationEnabled = settingBool(kAnimationEnabled, defaults.animationEnabled);
    m_state.idleMotionEnabled = settingBool(kIdleMotionEnabled, defaults.idleMotionEnabled);
    m_state.alwaysOnTop = settingBool(kAlwaysOnTop, defaults.alwaysOnTop);
    m_state.currentAnimationState = AnimationState::Idle;
    m_animationStateMachine.reset();
    m_animationStateMachine.configure(m_state.animationEnabled, m_state.idleMotionEnabled);

    return saveSettings();
}

bool CompanionManager::saveSettings()
{
    if (!m_storage.isOpen()) {
        return false;
    }

    m_state.companionVisible = true;
    saveBool(kCompanionVisible, true);
    saveBool(kPanelVisible, m_state.panelVisible);
    saveBool(kPanelDefaultOpen, m_state.panelDefaultOpen);
    saveBool(kCaptionsVisible, m_state.captionsVisible);
    saveString(kMode, toString(m_state.currentMode));
    saveInt(kAnchorX, m_state.anchorX);
    saveInt(kAnchorY, m_state.anchorY);
    saveBool(kLocked, m_state.companionLocked);
    saveInt(kCaptionFontSize, m_state.captionFontSize);
    saveDouble(kCaptionOpacity, m_state.captionOpacity);
    saveInt(kCaptionMaxLines, m_state.captionMaxLines);
    saveInt(kCaptionWidth, m_state.captionWidth);
    saveInt(kCaptionWidthLegacy, m_state.captionWidth);
    saveInt(kCaptionHeight, m_state.captionHeight);
    saveInt(kCaptionX, m_state.captionX);
    saveInt(kCaptionY, m_state.captionY);
    saveBool(kCaptionDetached, m_state.captionDetached);
    saveBool(kCaptionLocked, m_state.captionLocked);
    saveBool(kTranslationEnabled, m_state.translationEnabled);
    saveString(kSourceLanguage, m_state.sourceLanguage);
    saveString(kTargetLanguage, m_state.targetLanguage);
    saveDouble(kScale, m_state.companionScale);
    saveString(kThemePack, m_state.themePack);
    saveBool(kAnimationEnabled, m_state.animationEnabled);
    saveBool(kIdleMotionEnabled, m_state.idleMotionEnabled);
    saveBool(kAlwaysOnTop, m_state.alwaysOnTop);
    return true;
}

const CompanionState &CompanionManager::state() const
{
    return m_state;
}

AnimationStateMachine &CompanionManager::animationStateMachine()
{
    return m_animationStateMachine;
}

CompanionVisualProfile CompanionManager::visualProfile() const
{
    return defaultVisualProfileForMode(m_state.currentMode);
}

CompanionAssetRegistry CompanionManager::assetRegistry() const
{
    return defaultCompanionAssetRegistry();
}

void CompanionManager::setCompanionVisible(bool visible)
{
    (void)visible;
    m_state.companionVisible = true;
    saveBool(kCompanionVisible, true);
}

void CompanionManager::setPanelVisible(bool visible)
{
    m_state.panelVisible = visible;
    saveBool(kPanelVisible, visible);
}

void CompanionManager::setPanelDefaultOpen(bool defaultOpen)
{
    m_state.panelDefaultOpen = defaultOpen;
    saveBool(kPanelDefaultOpen, defaultOpen);
}

void CompanionManager::setCaptionsVisible(bool visible)
{
    m_state.captionsVisible = visible;
    saveBool(kCaptionsVisible, visible);
}

void CompanionManager::setMode(CompanionMode mode)
{
    m_state.currentMode = mode;
    saveString(kMode, toString(mode));
    if (m_state.currentAnimationState == AnimationState::Idle) {
        m_animationStateMachine.setState(visualProfile().defaultAnimation);
        syncAnimationState();
    }
}

void CompanionManager::setMicrophoneEnabled(bool enabled)
{
    m_state.microphoneEnabled = enabled;
    m_animationStateMachine.onListeningChanged(enabled);
    syncAnimationState();
}

void CompanionManager::setTranslationEnabled(bool enabled)
{
    m_state.translationEnabled = enabled;
    saveBool(kTranslationEnabled, enabled);
}

void CompanionManager::setLanguages(const std::string &sourceLanguage, const std::string &targetLanguage)
{
    m_state.sourceLanguage = sourceLanguage.empty() ? "auto" : sourceLanguage;
    m_state.targetLanguage = targetLanguage.empty() ? "en" : targetLanguage;
    saveString(kSourceLanguage, m_state.sourceLanguage);
    saveString(kTargetLanguage, m_state.targetLanguage);
}

void CompanionManager::setCaptionFontSize(int fontSize)
{
    m_state.captionFontSize = clampInt(fontSize, 12, 48);
    saveInt(kCaptionFontSize, m_state.captionFontSize);
}

void CompanionManager::setCaptionOpacity(double opacity)
{
    m_state.captionOpacity = clampDouble(opacity, 0.2, 1.0);
    saveDouble(kCaptionOpacity, m_state.captionOpacity);
}

void CompanionManager::setCaptionMaxLines(int maxLines)
{
    m_state.captionMaxLines = clampInt(maxLines, 1, 5);
    saveInt(kCaptionMaxLines, m_state.captionMaxLines);
}

void CompanionManager::setCaptionWidth(int width)
{
    m_state.captionWidth = clampInt(width, 260, 1200);
    saveInt(kCaptionWidth, m_state.captionWidth);
    saveInt(kCaptionWidthLegacy, m_state.captionWidth);
}

void CompanionManager::setCaptionHeight(int height)
{
    m_state.captionHeight = clampInt(height, 80, 500);
    saveInt(kCaptionHeight, m_state.captionHeight);
}

void CompanionManager::setCaptionPosition(int x, int y)
{
    m_state.captionX = x;
    m_state.captionY = y;
    saveInt(kCaptionX, x);
    saveInt(kCaptionY, y);
}

void CompanionManager::setCaptionSize(int width, int height)
{
    setCaptionWidth(width);
    setCaptionHeight(height);
}

void CompanionManager::setCaptionGeometry(int x, int y, int width, int height)
{
    setCaptionPosition(x, y);
    setCaptionSize(width, height);
}

void CompanionManager::resetCaptionGeometry()
{
    CompanionState defaults;
    setCaptionGeometry(defaults.captionX, defaults.captionY, defaults.captionWidth, defaults.captionHeight);
}

void CompanionManager::setCaptionDetached(bool detached)
{
    m_state.captionDetached = detached;
    saveBool(kCaptionDetached, detached);
}

void CompanionManager::setCaptionLocked(bool locked)
{
    m_state.captionLocked = locked;
    saveBool(kCaptionLocked, locked);
}

void CompanionManager::setAnchorPosition(int x, int y)
{
    m_state.anchorX = x;
    m_state.anchorY = y;
    saveInt(kAnchorX, x);
    saveInt(kAnchorY, y);
}

void CompanionManager::resetAnchorPosition()
{
    CompanionState defaults;
    setAnchorPosition(defaults.anchorX, defaults.anchorY);
}

void CompanionManager::setCompanionLocked(bool locked)
{
    m_state.companionLocked = locked;
    saveBool(kLocked, locked);
}

void CompanionManager::setCompanionScale(double scale)
{
    m_state.companionScale = clampDouble(scale, 0.65, 1.6);
    saveDouble(kScale, m_state.companionScale);
}

void CompanionManager::setThemePack(const std::string &themePack)
{
    m_state.themePack = themePack.empty() ? "default" : themePack;
    saveString(kThemePack, m_state.themePack);
}

void CompanionManager::setAnimationEnabled(bool enabled)
{
    m_state.animationEnabled = enabled;
    saveBool(kAnimationEnabled, enabled);
    m_animationStateMachine.configure(m_state.animationEnabled, m_state.idleMotionEnabled);
    syncAnimationState();
}

void CompanionManager::setIdleMotionEnabled(bool enabled)
{
    m_state.idleMotionEnabled = enabled;
    saveBool(kIdleMotionEnabled, enabled);
    m_animationStateMachine.configure(m_state.animationEnabled, m_state.idleMotionEnabled);
}

void CompanionManager::setAlwaysOnTop(bool alwaysOnTop)
{
    m_state.alwaysOnTop = alwaysOnTop;
    saveBool(kAlwaysOnTop, alwaysOnTop);
}

void CompanionManager::resetVisualSettings()
{
    CompanionState defaults;
    setCompanionScale(defaults.companionScale);
    setThemePack(defaults.themePack);
    setAnimationEnabled(defaults.animationEnabled);
    setIdleMotionEnabled(defaults.idleMotionEnabled);
    setAlwaysOnTop(defaults.alwaysOnTop);
}

void CompanionManager::setAnimationState(AnimationState state)
{
    m_animationStateMachine.setState(state);
    syncAnimationState();
}

void CompanionManager::onCaptionUpdated()
{
    m_animationStateMachine.onCaptionUpdated(m_state.currentMode);
    syncAnimationState();
}

void CompanionManager::syncAnimationState()
{
    m_state.currentAnimationState = m_animationStateMachine.state();
}

bool CompanionManager::settingBool(const char *key, bool defaultValue)
{
    const auto value = m_storage.getSetting(key);
    if (!value.has_value()) {
        persistDefault(key, boolText(defaultValue));
        return defaultValue;
    }
    return parseBool(*value, defaultValue);
}

int CompanionManager::settingInt(const char *key, int defaultValue)
{
    const auto value = m_storage.getSetting(key);
    if (!value.has_value()) {
        persistDefault(key, std::to_string(defaultValue));
        return defaultValue;
    }

    try {
        return std::stoi(*value);
    } catch (...) {
        return defaultValue;
    }
}

double CompanionManager::settingDouble(const char *key, double defaultValue)
{
    const auto value = m_storage.getSetting(key);
    if (!value.has_value()) {
        persistDefault(key, std::to_string(defaultValue));
        return defaultValue;
    }

    try {
        return std::stod(*value);
    } catch (...) {
        return defaultValue;
    }
}

std::string CompanionManager::settingString(const char *key, const std::string &defaultValue)
{
    const auto value = m_storage.getSetting(key);
    if (!value.has_value()) {
        persistDefault(key, defaultValue);
        return defaultValue;
    }
    return *value;
}

void CompanionManager::persistDefault(const char *key, const std::string &value)
{
    m_storage.setSetting(key, value);
}

void CompanionManager::saveBool(const char *key, bool value)
{
    m_storage.setSetting(key, boolText(value));
}

void CompanionManager::saveInt(const char *key, int value)
{
    m_storage.setSetting(key, std::to_string(value));
}

void CompanionManager::saveDouble(const char *key, double value)
{
    m_storage.setSetting(key, std::to_string(value));
}

void CompanionManager::saveString(const char *key, const std::string &value)
{
    m_storage.setSetting(key, value);
}

} // namespace local_jarvis::companion
