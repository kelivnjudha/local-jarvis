#pragma once

#include "CaptionFormatter.h"
#include "CaptionState.h"

#include <cstddef>
#include <string>

namespace local_jarvis::storage {
class Storage;
}

namespace local_jarvis::caption {

class CaptionManager {
public:
    explicit CaptionManager(storage::Storage *storage = nullptr);
    explicit CaptionManager(storage::Storage &storage);

    [[nodiscard]] const CaptionState &state() const;
    [[nodiscard]] std::string currentDisplayText() const;

    bool loadSettings();
    bool saveSettings();

    void setCaptionMode(CaptionMode mode);
    void setCaptionsEnabled(bool enabled);
    void setSourceLanguage(const std::string &languageCode);
    void setTargetLanguage(const std::string &languageCode);
    void setShowSpeaker(bool enabled);
    void setMaxLines(int maxLines);
    void setMaxCharacters(int maxCharacters);
    void addSegment(const CaptionSegment &segment);

private:
    [[nodiscard]] bool storageReady() const;
    [[nodiscard]] bool settingBool(const char *key, bool defaultValue);
    [[nodiscard]] int settingInt(const char *key, int defaultValue);
    [[nodiscard]] std::string settingString(const char *key, const std::string &defaultValue);
    void saveBool(const char *key, bool value);
    void saveInt(const char *key, int value);
    void saveString(const char *key, const std::string &value);
    void touchUpdatedAt();
    void trimLatestSegments();

    storage::Storage *m_storage = nullptr;
    CaptionState m_state {};
    CaptionFormatter m_formatter;
    std::size_t m_maxStoredSegments = 8;
};

} // namespace local_jarvis::caption
