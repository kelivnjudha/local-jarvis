#pragma once

#include <cstdint>
#include <string>

namespace local_jarvis::caption {

enum class CaptionMode {
    Off,
    OriginalOnly,
    TranslationOnly,
    OriginalAndTranslation,
    CleanSummary
};

enum class CaptionSource {
    Unknown,
    Microphone,
    SystemAudio
};

enum class CaptionSourceDisplayMode {
    CombinedChronological,
    SystemOnly,
    MicrophoneOnly,
    PreferSystemAudio,
    PreferMicrophone
};

struct CaptionSegment {
    std::string id;
    std::string speaker;
    std::string originalText;
    std::string translatedText;
    std::string summaryText;
    std::string detectedLanguage;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    bool isFinal = true;
    CaptionSource source = CaptionSource::Unknown;
};

[[nodiscard]] std::string toString(CaptionMode mode);
[[nodiscard]] CaptionMode captionModeFromString(const std::string &value);
[[nodiscard]] std::string displayName(CaptionMode mode);
[[nodiscard]] std::string toString(CaptionSource source);
[[nodiscard]] CaptionSource captionSourceFromString(const std::string &value);
[[nodiscard]] std::string displayName(CaptionSource source);
[[nodiscard]] std::string labelForSource(CaptionSource source);
[[nodiscard]] std::string toString(CaptionSourceDisplayMode mode);
[[nodiscard]] CaptionSourceDisplayMode captionSourceDisplayModeFromString(const std::string &value);
[[nodiscard]] std::string displayName(CaptionSourceDisplayMode mode);

} // namespace local_jarvis::caption
