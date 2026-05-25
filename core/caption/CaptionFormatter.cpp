#include "CaptionFormatter.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace local_jarvis::caption {
namespace {

std::string firstNonEmpty(
    const std::string &preferred,
    const std::string &fallback,
    const std::string &lastFallback = {})
{
    if (!preferred.empty()) {
        return preferred;
    }
    if (!fallback.empty()) {
        return fallback;
    }
    return lastFallback;
}

std::string trimTrailingNewlines(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

std::string uppercaseAscii(std::string value)
{
    for (char &character : value) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return value;
}

bool isUtf8ContinuationByte(unsigned char value)
{
    return (value & 0xC0) == 0x80;
}

} // namespace

std::string toString(CaptionMode mode)
{
    switch (mode) {
    case CaptionMode::Off:
        return "Off";
    case CaptionMode::OriginalOnly:
        return "OriginalOnly";
    case CaptionMode::TranslationOnly:
        return "TranslationOnly";
    case CaptionMode::OriginalAndTranslation:
        return "OriginalAndTranslation";
    case CaptionMode::CleanSummary:
        return "CleanSummary";
    }
    return "OriginalAndTranslation";
}

CaptionMode captionModeFromString(const std::string &value)
{
    if (value == "Off") {
        return CaptionMode::Off;
    }
    if (value == "OriginalOnly") {
        return CaptionMode::OriginalOnly;
    }
    if (value == "TranslationOnly") {
        return CaptionMode::TranslationOnly;
    }
    if (value == "CleanSummary") {
        return CaptionMode::CleanSummary;
    }
    return CaptionMode::OriginalAndTranslation;
}

std::string displayName(CaptionMode mode)
{
    switch (mode) {
    case CaptionMode::Off:
        return "Off";
    case CaptionMode::OriginalOnly:
        return "Original only";
    case CaptionMode::TranslationOnly:
        return "English only";
    case CaptionMode::OriginalAndTranslation:
        return "Original + English";
    case CaptionMode::CleanSummary:
        return "Summary";
    }
    return "Original + English";
}

std::string CaptionFormatter::format(const CaptionState &state) const
{
    return format(state, state.latestSegments);
}

std::string CaptionFormatter::format(const CaptionState &state, const std::vector<CaptionSegment> &segments) const
{
    if (!state.captionsEnabled || state.captionMode == CaptionMode::Off || segments.empty()) {
        return {};
    }

    std::string output;
    for (auto iterator = segments.rbegin(); iterator != segments.rend(); ++iterator) {
        const std::string segmentText = formatSegment(state, *iterator);
        if (segmentText.empty()) {
            continue;
        }
        if (!output.empty()) {
            output += '\n';
        }
        output += segmentText;
        if (state.maxLines > 0) {
            const int lineCount = static_cast<int>(std::count(output.begin(), output.end(), '\n')) + 1;
            if (lineCount >= state.maxLines) {
                break;
            }
        }
    }

    return limitOutput(output, state.maxLines, state.maxCharacters);
}

std::string CaptionFormatter::formatSegment(const CaptionState &state, const CaptionSegment &segment) const
{
    const std::string prefix = speakerPrefix(state, segment);
    switch (state.captionMode) {
    case CaptionMode::Off:
        return {};
    case CaptionMode::OriginalOnly: {
        const std::string text = firstNonEmpty(segment.originalText, segment.translatedText, segment.summaryText);
        return text.empty() ? std::string {} : prefix + text;
    }
    case CaptionMode::TranslationOnly: {
        const std::string text = firstNonEmpty(segment.translatedText, segment.originalText, segment.summaryText);
        return text.empty() ? std::string {} : targetLanguagePrefix(state) + text;
    }
    case CaptionMode::OriginalAndTranslation: {
        const std::string original = firstNonEmpty(segment.originalText, segment.translatedText, segment.summaryText);
        const std::string translated = firstNonEmpty(segment.translatedText, segment.originalText, segment.summaryText);
        if (original.empty() && translated.empty()) {
            return {};
        }
        if (translated.empty()) {
            return prefix + original;
        }
        if (original.empty()) {
            return targetLanguagePrefix(state) + translated;
        }
        return prefix + original + '\n' + targetLanguagePrefix(state) + translated;
    }
    case CaptionMode::CleanSummary: {
        const std::string text = firstNonEmpty(segment.summaryText, segment.translatedText, segment.originalText);
        return text.empty() ? std::string {} : "Key point: " + text;
    }
    }
    return {};
}

std::string CaptionFormatter::speakerPrefix(const CaptionState &state, const CaptionSegment &segment) const
{
    if (!state.showSpeaker || segment.speaker.empty()) {
        return {};
    }
    return segment.speaker + ": ";
}

std::string CaptionFormatter::targetLanguagePrefix(const CaptionState &state) const
{
    std::string language = state.targetLanguage.empty() ? "en" : state.targetLanguage;
    if (language == "en") {
        language = "EN";
    } else {
        language = uppercaseAscii(language);
    }
    return language + ": ";
}

std::string CaptionFormatter::limitOutput(const std::string &text, int maxLines, int maxCharacters) const
{
    std::stringstream stream(text);
    std::string line;
    std::string limited;
    int emittedLines = 0;
    while (std::getline(stream, line)) {
        if (maxLines > 0 && emittedLines >= maxLines) {
            break;
        }
        if (!limited.empty()) {
            limited += '\n';
        }
        limited += line;
        ++emittedLines;
    }

    limited = trimTrailingNewlines(limited);
    if (maxCharacters > 0) {
        limited = truncateUtf8(limited, maxCharacters);
    }
    return limited;
}

std::string CaptionFormatter::truncateUtf8(const std::string &text, int maxCharacters) const
{
    if (maxCharacters <= 0 || static_cast<int>(text.size()) <= maxCharacters) {
        return text;
    }
    if (maxCharacters <= 3) {
        return text.substr(0, static_cast<std::size_t>(maxCharacters));
    }

    std::size_t end = static_cast<std::size_t>(maxCharacters - 3);
    while (end > 0 && isUtf8ContinuationByte(static_cast<unsigned char>(text[end]))) {
        --end;
    }
    return text.substr(0, end) + "...";
}

} // namespace local_jarvis::caption
