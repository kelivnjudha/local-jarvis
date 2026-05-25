#include "CaptionManager.h"

#include "storage/Storage.h"

#include <algorithm>
#include <chrono>
#include <string>

namespace local_jarvis::caption {
namespace {

constexpr const char *kCaptionEnabled = "caption.enabled";
constexpr const char *kCaptionMode = "caption.mode";
constexpr const char *kShowSpeaker = "caption.show_speaker";
constexpr const char *kMaxLines = "caption.max_lines";
constexpr const char *kMaxCharacters = "caption.max_characters";
constexpr const char *kSourceLanguage = "caption.source_language";
constexpr const char *kTargetLanguage = "caption.target_language";

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
    return m_formatter.format(m_state);
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
    m_state.maxLines = clampInt(settingInt(kMaxLines, defaults.maxLines), 1, 8);
    m_state.maxCharacters = clampInt(settingInt(kMaxCharacters, defaults.maxCharacters), 40, 1000);
    m_state.sourceLanguage = settingString(kSourceLanguage, defaults.sourceLanguage);
    m_state.targetLanguage = settingString(kTargetLanguage, defaults.targetLanguage);
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
    saveInt(kMaxLines, m_state.maxLines);
    saveInt(kMaxCharacters, m_state.maxCharacters);
    saveString(kSourceLanguage, m_state.sourceLanguage);
    saveString(kTargetLanguage, m_state.targetLanguage);
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

void CaptionManager::addSegment(const CaptionSegment &segment)
{
    m_state.latestSegments.push_back(segment);
    trimLatestSegments();
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

} // namespace local_jarvis::caption
