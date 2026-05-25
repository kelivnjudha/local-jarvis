#pragma once

#include "AnimationStateMachine.h"
#include "CompanionState.h"

#include <optional>
#include <string>

namespace local_jarvis::storage {
class Storage;
}

namespace local_jarvis::companion {

class CompanionManager {
public:
    explicit CompanionManager(storage::Storage &storage);

    bool loadSettings();
    bool saveSettings();

    [[nodiscard]] const CompanionState &state() const;
    [[nodiscard]] AnimationStateMachine &animationStateMachine();

    void setCompanionVisible(bool visible);
    void setPanelVisible(bool visible);
    void setCaptionsVisible(bool visible);
    void setMode(CompanionMode mode);
    void setMicrophoneEnabled(bool enabled);
    void setTranslationEnabled(bool enabled);
    void setLanguages(const std::string &sourceLanguage, const std::string &targetLanguage);
    void setCaptionFontSize(int fontSize);
    void setCaptionOpacity(double opacity);
    void setCaptionMaxLines(int maxLines);
    void setCaptionWidth(int width);
    void setAnchorPosition(int x, int y);
    void resetAnchorPosition();
    void setCompanionLocked(bool locked);
    void setAnimationState(AnimationState state);
    void syncAnimationState();

private:
    [[nodiscard]] bool settingBool(const char *key, bool defaultValue);
    [[nodiscard]] int settingInt(const char *key, int defaultValue);
    [[nodiscard]] double settingDouble(const char *key, double defaultValue);
    [[nodiscard]] std::string settingString(const char *key, const std::string &defaultValue);

    void persistDefault(const char *key, const std::string &value);
    void saveBool(const char *key, bool value);
    void saveInt(const char *key, int value);
    void saveDouble(const char *key, double value);
    void saveString(const char *key, const std::string &value);

    storage::Storage &m_storage;
    CompanionState m_state {};
    AnimationStateMachine m_animationStateMachine;
};

} // namespace local_jarvis::companion
