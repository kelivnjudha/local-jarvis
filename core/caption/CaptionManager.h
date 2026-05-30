#pragma once

#include "CaptionFormatter.h"
#include "CaptionState.h"

#include <cstddef>
#include <chrono>
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
    [[nodiscard]] std::size_t duplicateSuppressedCount() const;

    bool loadSettings();
    bool saveSettings();

    void setCaptionMode(CaptionMode mode);
    void setCaptionsEnabled(bool enabled);
    void setSourceLanguage(const std::string &languageCode);
    void setTargetLanguage(const std::string &languageCode);
    void setShowSpeaker(bool enabled);
    void setMaxLines(int maxLines);
    void setMaxCharacters(int maxCharacters);
    void setHoldMs(int holdMs);
    void setSuppressDuplicates(bool enabled);
    void setDuplicateWindowMs(int duplicateWindowMs);
    void setClearOnAsrOff(bool enabled);
    void addSegment(const CaptionSegment &segment);
    void clearSegments();

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
    [[nodiscard]] bool shouldSuppressDuplicate(const CaptionSegment &segment);
    [[nodiscard]] std::string segmentTextKey(const CaptionSegment &segment) const;

    storage::Storage *m_storage = nullptr;
    CaptionState m_state {};
    CaptionFormatter m_formatter;
    std::size_t m_maxStoredSegments = 8;
    mutable std::string m_lastDisplayText;
    mutable std::chrono::steady_clock::time_point m_lastUsefulDisplayAt {};
    std::string m_lastAcceptedSegmentText;
    std::chrono::steady_clock::time_point m_lastAcceptedSegmentAt {};
    std::size_t m_duplicateSuppressedCount = 0;
};

} // namespace local_jarvis::caption
