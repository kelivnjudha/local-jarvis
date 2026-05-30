#pragma once

#include "CaptionCleaner.h"
#include "CaptionFormatter.h"
#include "CaptionMerger.h"
#include "CaptionState.h"

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
    [[nodiscard]] std::size_t crossSourceDuplicateSuppressedCount() const;
    [[nodiscard]] CaptionQualityStats qualityStats() const;
    [[nodiscard]] std::string cleanTranscriptText(const std::string &text) const;
    [[nodiscard]] bool isNonContentText(const std::string &text) const;

    bool loadSettings();
    bool saveSettings();

    void setCaptionMode(CaptionMode mode);
    void setCaptionsEnabled(bool enabled);
    void setSourceLanguage(const std::string &languageCode);
    void setTargetLanguage(const std::string &languageCode);
    void setShowSpeaker(bool enabled);
    void setShowSourceLabels(bool enabled);
    void setSourceDisplayMode(CaptionSourceDisplayMode mode);
    void setMaxLines(int maxLines);
    void setMaxCharacters(int maxCharacters);
    void setHoldMs(int holdMs);
    void setSuppressDuplicates(bool enabled);
    void setDuplicateWindowMs(int duplicateWindowMs);
    void setClearOnAsrOff(bool enabled);
    void setCleaningEnabled(bool enabled);
    void setMergeShortSegments(bool enabled);
    void setMergeMaxGapMs(int maxGapMs);
    void setMergeMaxCharacters(int maxCharacters);
    void setAutoPunctuationLight(bool enabled);
    void recordRejectedCaption();
    bool addSegment(const CaptionSegment &segment);
    void clearSegments();

private:
    enum class DuplicateDecision {
        Accept,
        Suppress,
        ReplaceWithPreferredSource
    };

    [[nodiscard]] bool storageReady() const;
    [[nodiscard]] bool settingBool(const char *key, bool defaultValue);
    [[nodiscard]] int settingInt(const char *key, int defaultValue);
    [[nodiscard]] std::string settingString(const char *key, const std::string &defaultValue);
    void saveBool(const char *key, bool value);
    void saveInt(const char *key, int value);
    void saveString(const char *key, const std::string &value);
    void touchUpdatedAt();
    void trimLatestSegments();
    [[nodiscard]] CaptionCleanerOptions cleanerOptions() const;
    [[nodiscard]] CaptionMergerOptions mergerOptions() const;
    void updateLastAcceptedSegment(const CaptionSegment &segment);
    [[nodiscard]] DuplicateDecision duplicateDecision(const CaptionSegment &segment) const;
    [[nodiscard]] std::string segmentTextKey(const CaptionSegment &segment) const;
    [[nodiscard]] std::string normalizedTextKey(const CaptionSegment &segment) const;
    [[nodiscard]] bool isNearDuplicateText(const std::string &left, const std::string &right) const;
    [[nodiscard]] bool systemAudioPreferredOver(CaptionSource existingSource, CaptionSource nextSource) const;

    storage::Storage *m_storage = nullptr;
    CaptionState m_state {};
    CaptionCleaner m_cleaner;
    CaptionFormatter m_formatter;
    CaptionMerger m_merger;
    std::size_t m_maxStoredSegments = 8;
    mutable std::string m_lastDisplayText;
    mutable std::chrono::steady_clock::time_point m_lastUsefulDisplayAt {};
    std::string m_lastAcceptedSegmentText;
    CaptionSource m_lastAcceptedSegmentSource = CaptionSource::Unknown;
    std::chrono::steady_clock::time_point m_lastAcceptedSegmentAt {};
    mutable CaptionQualityStats m_qualityStats;
};

} // namespace local_jarvis::caption
