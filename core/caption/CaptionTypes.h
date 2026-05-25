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
};

[[nodiscard]] std::string toString(CaptionMode mode);
[[nodiscard]] CaptionMode captionModeFromString(const std::string &value);
[[nodiscard]] std::string displayName(CaptionMode mode);

} // namespace local_jarvis::caption
