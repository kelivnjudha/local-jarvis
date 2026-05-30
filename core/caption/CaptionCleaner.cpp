#include "CaptionCleaner.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace local_jarvis::caption {
namespace {

bool isSentenceEndingPunctuation(char value)
{
    return value == '.' || value == '!' || value == '?' || value == ':' || value == ';';
}

bool isRepeatablePunctuation(char value)
{
    return value == '.' || value == '!' || value == '?' || value == ',' || value == ':' || value == ';';
}

} // namespace

CaptionCleanerResult CaptionCleaner::cleanText(const std::string &text, const CaptionCleanerOptions &options) const
{
    CaptionCleanerResult result;
    std::string cleaned = trimAscii(text);
    if (isNonContentToken(cleaned)) {
        result.rejected = true;
        result.changed = cleaned != text;
        return result;
    }

    cleaned = normalizeWhitespace(cleaned);
    if (options.enabled) {
        cleaned = normalizeRepeatedPunctuation(cleaned);
        if (options.capitalizeFirstLetter) {
            cleaned = capitalizeFirstAsciiLetter(std::move(cleaned));
        }
        if (options.addLightPunctuation) {
            cleaned = addSentencePunctuation(std::move(cleaned));
        }
    }

    if (isNonContentToken(cleaned)) {
        result.rejected = true;
        result.changed = cleaned != text;
        return result;
    }

    result.text = std::move(cleaned);
    result.changed = result.text != text;
    return result;
}

std::optional<CaptionSegment> CaptionCleaner::cleanSegment(
    const CaptionSegment &segment,
    const CaptionCleanerOptions &options) const
{
    CaptionSegment cleaned = segment;
    cleaned.originalText = cleanText(segment.originalText, options).text;
    cleaned.translatedText = cleanText(segment.translatedText, options).text;
    cleaned.summaryText = cleanText(segment.summaryText, options).text;
    cleaned.speaker = normalizeWhitespace(trimAscii(segment.speaker));
    cleaned.detectedLanguage = normalizeWhitespace(trimAscii(segment.detectedLanguage));
    if (!hasContent(cleaned)) {
        return std::nullopt;
    }
    return cleaned;
}

bool CaptionCleaner::hasContent(const CaptionSegment &segment) const
{
    return !isNonContentToken(segment.originalText)
        || !isNonContentToken(segment.translatedText)
        || !isNonContentToken(segment.summaryText);
}

bool CaptionCleaner::isNonContentToken(const std::string &text) const
{
    const std::string trimmed = trimAscii(text);
    if (trimmed.empty()) {
        return true;
    }

    std::string normalized;
    normalized.reserve(trimmed.size());
    for (const unsigned char character : trimmed) {
        if (std::isspace(character) == 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return normalized == "[blank_audio]";
}

std::string CaptionCleaner::trimAscii(const std::string &text) const
{
    const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string CaptionCleaner::normalizeWhitespace(const std::string &text) const
{
    std::string normalized;
    normalized.reserve(text.size());
    bool previousWasSpace = false;
    for (const unsigned char character : text) {
        if (std::isspace(character) != 0) {
            if (!previousWasSpace && !normalized.empty()) {
                normalized.push_back(' ');
            }
            previousWasSpace = true;
        } else {
            normalized.push_back(static_cast<char>(character));
            previousWasSpace = false;
        }
    }
    if (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
    }
    return normalized;
}

std::string CaptionCleaner::normalizeRepeatedPunctuation(const std::string &text) const
{
    std::string normalized;
    normalized.reserve(text.size());
    for (const char character : text) {
        if (!normalized.empty()
            && character == normalized.back()
            && isRepeatablePunctuation(character)) {
            continue;
        }
        normalized.push_back(character);
    }
    return normalized;
}

std::string CaptionCleaner::capitalizeFirstAsciiLetter(std::string text) const
{
    for (char &character : text) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (std::isalpha(value) != 0) {
            character = static_cast<char>(std::toupper(value));
            break;
        }
        if (std::isalnum(value) != 0) {
            break;
        }
    }
    return text;
}

std::string CaptionCleaner::addSentencePunctuation(std::string text) const
{
    if (text.empty()) {
        return text;
    }

    char last = text.back();
    if ((last == '"' || last == '\'') && text.size() >= 2) {
        last = text[text.size() - 2];
    }
    if (isSentenceEndingPunctuation(last)) {
        return text;
    }
    if (std::isalnum(static_cast<unsigned char>(last)) != 0 || last == ')' || last == ']') {
        text.push_back('.');
    }
    return text;
}

} // namespace local_jarvis::caption
