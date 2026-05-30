#pragma once

#include "CaptionTypes.h"

#include <optional>
#include <string>

namespace local_jarvis::caption {

struct CaptionCleanerOptions {
    bool enabled = true;
    bool capitalizeFirstLetter = true;
    bool addLightPunctuation = true;
};

struct CaptionCleanerResult {
    std::string text;
    bool rejected = false;
    bool changed = false;
};

class CaptionCleaner {
public:
    [[nodiscard]] CaptionCleanerResult cleanText(
        const std::string &text,
        const CaptionCleanerOptions &options = {}) const;
    [[nodiscard]] std::optional<CaptionSegment> cleanSegment(
        const CaptionSegment &segment,
        const CaptionCleanerOptions &options = {}) const;
    [[nodiscard]] bool hasContent(const CaptionSegment &segment) const;
    [[nodiscard]] bool isNonContentToken(const std::string &text) const;

private:
    [[nodiscard]] std::string trimAscii(const std::string &text) const;
    [[nodiscard]] std::string normalizeWhitespace(const std::string &text) const;
    [[nodiscard]] std::string normalizeRepeatedPunctuation(const std::string &text) const;
    [[nodiscard]] std::string capitalizeFirstAsciiLetter(std::string text) const;
    [[nodiscard]] std::string addSentencePunctuation(std::string text) const;
};

} // namespace local_jarvis::caption
