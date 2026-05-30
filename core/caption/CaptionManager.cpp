#include "CaptionManager.h"

#include "storage/Storage.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>

namespace local_jarvis::caption {
namespace {

constexpr const char *kCaptionEnabled = "caption.enabled";
constexpr const char *kCaptionMode = "caption.mode";
constexpr const char *kShowSpeaker = "caption.show_speaker";
constexpr const char *kShowSourceLabels = "caption.source_labels.enabled";
constexpr const char *kSourceDisplayMode = "caption.source_display_mode";
constexpr const char *kMaxLines = "caption.max_lines";
constexpr const char *kMaxCharacters = "caption.max_characters";
constexpr const char *kSourceLanguage = "caption.source_language";
constexpr const char *kTargetLanguage = "caption.target_language";
constexpr const char *kHoldMs = "caption.hold_ms";
constexpr const char *kSuppressDuplicates = "caption.suppress_duplicates";
constexpr const char *kDuplicateWindowMs = "caption.duplicate_window_ms";
constexpr const char *kClearOnAsrOff = "caption.clear_on_asr_off";

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

std::string nowText()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::to_string(millis);
}

std::string trimAscii(std::string value)
{
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string lowercaseAscii(std::string value)
{
    for (char &character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

bool isAsciiWordCharacter(unsigned char value)
{
    return std::isalnum(value) != 0;
}

} // namespace

CaptionManager::CaptionManager(storage::Storage *storage)
    : m_storage(storage)
{
}

CaptionManager::CaptionManager(storage::Storage &storage)
    : m_storage(&storage)
{
}

const CaptionState &CaptionManager::state() const
{
    return m_state;
}

std::string CaptionManager::currentDisplayText() const
{
    const std::string formatted = m_formatter.format(m_state);
    const auto now = std::chrono::steady_clock::now();
    if (!formatted.empty()) {
        if (formatted != m_lastDisplayText) {
            m_lastDisplayText = formatted;
        }
        m_lastUsefulDisplayAt = now;
        return formatted;
    }

    if (!m_state.captionsEnabled || m_state.captionMode == CaptionMode::Off) {
        return {};
    }

    if (m_state.clearOnAsrOff || m_lastDisplayText.empty() || m_state.holdMs <= 0) {
        return {};
    }

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUsefulDisplayAt).count();
    if (elapsedMs <= m_state.holdMs) {
        return m_lastDisplayText;
    }
    return {};
}

std::size_t CaptionManager::duplicateSuppressedCount() const
{
    return m_duplicateSuppressedCount;
}

std::size_t CaptionManager::crossSourceDuplicateSuppressedCount() const
{
    return m_crossSourceDuplicateSuppressedCount;
}

bool CaptionManager::loadSettings()
{
    if (!storageReady()) {
        return false;
    }

    CaptionState defaults;
    m_state.captionsEnabled = settingBool(kCaptionEnabled, defaults.captionsEnabled);
    m_state.captionMode = captionModeFromString(settingString(kCaptionMode, toString(defaults.captionMode)));
    m_state.showSpeaker = settingBool(kShowSpeaker, defaults.showSpeaker);
    m_state.showSourceLabels = settingBool(kShowSourceLabels, defaults.showSourceLabels);
    m_state.sourceDisplayMode = captionSourceDisplayModeFromString(settingString(kSourceDisplayMode, toString(defaults.sourceDisplayMode)));
    m_state.maxLines = clampInt(settingInt(kMaxLines, defaults.maxLines), 1, 8);
    m_state.maxCharacters = clampInt(settingInt(kMaxCharacters, defaults.maxCharacters), 40, 1000);
    m_state.sourceLanguage = settingString(kSourceLanguage, defaults.sourceLanguage);
    m_state.targetLanguage = settingString(kTargetLanguage, defaults.targetLanguage);
    m_state.holdMs = clampInt(settingInt(kHoldMs, defaults.holdMs), 0, 30000);
    m_state.suppressDuplicates = settingBool(kSuppressDuplicates, defaults.suppressDuplicates);
    m_state.duplicateWindowMs = clampInt(settingInt(kDuplicateWindowMs, defaults.duplicateWindowMs), 0, 60000);
    m_state.clearOnAsrOff = settingBool(kClearOnAsrOff, defaults.clearOnAsrOff);
    if (m_state.sourceLanguage.empty()) {
        m_state.sourceLanguage = defaults.sourceLanguage;
    }
    if (m_state.targetLanguage.empty()) {
        m_state.targetLanguage = defaults.targetLanguage;
    }
    return saveSettings();
}

bool CaptionManager::saveSettings()
{
    if (!storageReady()) {
        return false;
    }

    saveBool(kCaptionEnabled, m_state.captionsEnabled);
    saveString(kCaptionMode, toString(m_state.captionMode));
    saveBool(kShowSpeaker, m_state.showSpeaker);
    saveBool(kShowSourceLabels, m_state.showSourceLabels);
    saveString(kSourceDisplayMode, toString(m_state.sourceDisplayMode));
    saveInt(kMaxLines, m_state.maxLines);
    saveInt(kMaxCharacters, m_state.maxCharacters);
    saveString(kSourceLanguage, m_state.sourceLanguage);
    saveString(kTargetLanguage, m_state.targetLanguage);
    saveInt(kHoldMs, m_state.holdMs);
    saveBool(kSuppressDuplicates, m_state.suppressDuplicates);
    saveInt(kDuplicateWindowMs, m_state.duplicateWindowMs);
    saveBool(kClearOnAsrOff, m_state.clearOnAsrOff);
    return true;
}

void CaptionManager::setCaptionMode(CaptionMode mode)
{
    m_state.captionMode = mode;
    saveString(kCaptionMode, toString(mode));
    touchUpdatedAt();
}

void CaptionManager::setCaptionsEnabled(bool enabled)
{
    m_state.captionsEnabled = enabled;
    saveBool(kCaptionEnabled, enabled);
    touchUpdatedAt();
}

void CaptionManager::setSourceLanguage(const std::string &languageCode)
{
    m_state.sourceLanguage = languageCode.empty() ? "auto" : languageCode;
    saveString(kSourceLanguage, m_state.sourceLanguage);
    touchUpdatedAt();
}

void CaptionManager::setTargetLanguage(const std::string &languageCode)
{
    m_state.targetLanguage = languageCode.empty() ? "en" : languageCode;
    saveString(kTargetLanguage, m_state.targetLanguage);
    touchUpdatedAt();
}

void CaptionManager::setShowSpeaker(bool enabled)
{
    m_state.showSpeaker = enabled;
    saveBool(kShowSpeaker, enabled);
    touchUpdatedAt();
}

void CaptionManager::setShowSourceLabels(bool enabled)
{
    m_state.showSourceLabels = enabled;
    saveBool(kShowSourceLabels, enabled);
    touchUpdatedAt();
}

void CaptionManager::setSourceDisplayMode(CaptionSourceDisplayMode mode)
{
    m_state.sourceDisplayMode = mode;
    saveString(kSourceDisplayMode, toString(mode));
    touchUpdatedAt();
}

void CaptionManager::setMaxLines(int maxLines)
{
    m_state.maxLines = clampInt(maxLines, 1, 8);
    saveInt(kMaxLines, m_state.maxLines);
    touchUpdatedAt();
}

void CaptionManager::setMaxCharacters(int maxCharacters)
{
    m_state.maxCharacters = clampInt(maxCharacters, 40, 1000);
    saveInt(kMaxCharacters, m_state.maxCharacters);
    touchUpdatedAt();
}

void CaptionManager::setHoldMs(int holdMs)
{
    m_state.holdMs = clampInt(holdMs, 0, 30000);
    saveInt(kHoldMs, m_state.holdMs);
    touchUpdatedAt();
}

void CaptionManager::setSuppressDuplicates(bool enabled)
{
    m_state.suppressDuplicates = enabled;
    saveBool(kSuppressDuplicates, enabled);
    touchUpdatedAt();
}

void CaptionManager::setDuplicateWindowMs(int duplicateWindowMs)
{
    m_state.duplicateWindowMs = clampInt(duplicateWindowMs, 0, 60000);
    saveInt(kDuplicateWindowMs, m_state.duplicateWindowMs);
    touchUpdatedAt();
}

void CaptionManager::setClearOnAsrOff(bool enabled)
{
    m_state.clearOnAsrOff = enabled;
    saveBool(kClearOnAsrOff, enabled);
    touchUpdatedAt();
}

bool CaptionManager::addSegment(const CaptionSegment &segment)
{
    const auto decision = duplicateDecision(segment);
    if (decision == DuplicateDecision::Suppress) {
        ++m_duplicateSuppressedCount;
        if (segment.source != m_lastAcceptedSegmentSource) {
            ++m_crossSourceDuplicateSuppressedCount;
        }
        touchUpdatedAt();
        return false;
    }

    const std::string normalizedKey = normalizedTextKey(segment);
    if (decision == DuplicateDecision::ReplaceWithPreferredSource) {
        ++m_duplicateSuppressedCount;
        ++m_crossSourceDuplicateSuppressedCount;
        bool replaced = false;
        for (auto iterator = m_state.latestSegments.rbegin(); iterator != m_state.latestSegments.rend(); ++iterator) {
            if (isNearDuplicateText(normalizedTextKey(*iterator), normalizedKey)
                && systemAudioPreferredOver(iterator->source, segment.source)) {
                *iterator = segment;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            m_state.latestSegments.push_back(segment);
        }
    } else {
        m_state.latestSegments.push_back(segment);
    }

    if (!normalizedKey.empty()) {
        m_lastAcceptedSegmentText = normalizedKey;
        m_lastAcceptedSegmentSource = segment.source;
        m_lastAcceptedSegmentAt = std::chrono::steady_clock::now();
    }
    trimLatestSegments();
    touchUpdatedAt();
    return true;
}

void CaptionManager::clearSegments()
{
    m_state.latestSegments.clear();
    touchUpdatedAt();
}

bool CaptionManager::storageReady() const
{
    return m_storage != nullptr && m_storage->isOpen();
}

bool CaptionManager::settingBool(const char *key, bool defaultValue)
{
    const auto value = m_storage->getSetting(key);
    if (!value.has_value()) {
        saveBool(key, defaultValue);
        return defaultValue;
    }
    return parseBool(*value, defaultValue);
}

int CaptionManager::settingInt(const char *key, int defaultValue)
{
    const auto value = m_storage->getSetting(key);
    if (!value.has_value()) {
        saveInt(key, defaultValue);
        return defaultValue;
    }
    try {
        return std::stoi(*value);
    } catch (...) {
        return defaultValue;
    }
}

std::string CaptionManager::settingString(const char *key, const std::string &defaultValue)
{
    const auto value = m_storage->getSetting(key);
    if (!value.has_value()) {
        saveString(key, defaultValue);
        return defaultValue;
    }
    return *value;
}

void CaptionManager::saveBool(const char *key, bool value)
{
    if (storageReady()) {
        m_storage->setSetting(key, boolText(value));
    }
}

void CaptionManager::saveInt(const char *key, int value)
{
    if (storageReady()) {
        m_storage->setSetting(key, std::to_string(value));
    }
}

void CaptionManager::saveString(const char *key, const std::string &value)
{
    if (storageReady()) {
        m_storage->setSetting(key, value);
    }
}

void CaptionManager::touchUpdatedAt()
{
    m_state.lastUpdatedAt = nowText();
}

void CaptionManager::trimLatestSegments()
{
    if (m_state.latestSegments.size() <= m_maxStoredSegments) {
        return;
    }
    const auto removeCount = m_state.latestSegments.size() - m_maxStoredSegments;
    m_state.latestSegments.erase(m_state.latestSegments.begin(), m_state.latestSegments.begin() + static_cast<std::ptrdiff_t>(removeCount));
}

CaptionManager::DuplicateDecision CaptionManager::duplicateDecision(const CaptionSegment &segment) const
{
    if (!m_state.suppressDuplicates || m_state.duplicateWindowMs <= 0) {
        return DuplicateDecision::Accept;
    }

    const std::string textKey = normalizedTextKey(segment);
    if (textKey.empty()
        || m_lastAcceptedSegmentText.empty()
        || m_lastAcceptedSegmentAt == std::chrono::steady_clock::time_point {}
        || !isNearDuplicateText(textKey, m_lastAcceptedSegmentText)) {
        return DuplicateDecision::Accept;
    }

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_lastAcceptedSegmentAt).count();
    if (elapsedMs > m_state.duplicateWindowMs) {
        return DuplicateDecision::Accept;
    }

    if (segment.source != m_lastAcceptedSegmentSource
        && systemAudioPreferredOver(m_lastAcceptedSegmentSource, segment.source)) {
        return DuplicateDecision::ReplaceWithPreferredSource;
    }

    return DuplicateDecision::Suppress;
}

std::string CaptionManager::segmentTextKey(const CaptionSegment &segment) const
{
    if (!segment.originalText.empty()) {
        return trimAscii(segment.originalText);
    }
    if (!segment.translatedText.empty()) {
        return trimAscii(segment.translatedText);
    }
    return trimAscii(segment.summaryText);
}

std::string CaptionManager::normalizedTextKey(const CaptionSegment &segment) const
{
    const std::string text = lowercaseAscii(segmentTextKey(segment));
    std::string normalized;
    bool previousWasSpace = false;
    for (const unsigned char character : text) {
        if (isAsciiWordCharacter(character)) {
            normalized.push_back(static_cast<char>(character));
            previousWasSpace = false;
        } else if (!previousWasSpace && !normalized.empty()) {
            normalized.push_back(' ');
            previousWasSpace = true;
        }
    }
    if (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
    }
    return normalized;
}

bool CaptionManager::isNearDuplicateText(const std::string &left, const std::string &right) const
{
    if (left.empty() || right.empty()) {
        return false;
    }
    if (left == right) {
        return true;
    }
    const auto shorterLength = std::min(left.size(), right.size());
    const auto longerLength = std::max(left.size(), right.size());
    if (shorterLength < 12 || longerLength == 0) {
        return false;
    }
    const double ratio = static_cast<double>(shorterLength) / static_cast<double>(longerLength);
    if (ratio < 0.82) {
        return false;
    }
    return left.find(right) != std::string::npos || right.find(left) != std::string::npos;
}

bool CaptionManager::systemAudioPreferredOver(CaptionSource existingSource, CaptionSource nextSource) const
{
    return nextSource == CaptionSource::SystemAudio && existingSource != CaptionSource::SystemAudio;
}

} // namespace local_jarvis::caption
