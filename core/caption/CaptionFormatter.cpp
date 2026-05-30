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

std::string toString(CaptionSource source)
{
    switch (source) {
    case CaptionSource::Microphone:
        return "Microphone";
    case CaptionSource::SystemAudio:
        return "SystemAudio";
    case CaptionSource::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

CaptionSource captionSourceFromString(const std::string &value)
{
    if (value == "Microphone" || value == "microphone" || value == "mic") {
        return CaptionSource::Microphone;
    }
    if (value == "SystemAudio" || value == "system_audio" || value == "system") {
        return CaptionSource::SystemAudio;
    }
    return CaptionSource::Unknown;
}

std::string displayName(CaptionSource source)
{
    switch (source) {
    case CaptionSource::Microphone:
        return "Microphone";
    case CaptionSource::SystemAudio:
        return "System audio";
    case CaptionSource::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string labelForSource(CaptionSource source)
{
    switch (source) {
    case CaptionSource::Microphone:
        return "Mic";
    case CaptionSource::SystemAudio:
        return "System";
    case CaptionSource::Unknown:
        return {};
    }
    return {};
}

std::string toString(CaptionSourceDisplayMode mode)
{
    switch (mode) {
    case CaptionSourceDisplayMode::CombinedChronological:
        return "Combined";
    case CaptionSourceDisplayMode::SystemOnly:
        return "SystemOnly";
    case CaptionSourceDisplayMode::MicrophoneOnly:
        return "MicrophoneOnly";
    case CaptionSourceDisplayMode::PreferSystemAudio:
        return "PreferSystem";
    case CaptionSourceDisplayMode::PreferMicrophone:
        return "PreferMicrophone";
    }
    return "Combined";
}

CaptionSourceDisplayMode captionSourceDisplayModeFromString(const std::string &value)
{
    if (value == "SystemOnly" || value == "system_only") {
        return CaptionSourceDisplayMode::SystemOnly;
    }
    if (value == "MicrophoneOnly" || value == "mic_only" || value == "microphone_only") {
        return CaptionSourceDisplayMode::MicrophoneOnly;
    }
    if (value == "PreferSystem" || value == "prefer_system") {
        return CaptionSourceDisplayMode::PreferSystemAudio;
    }
    if (value == "PreferMicrophone" || value == "prefer_mic" || value == "prefer_microphone") {
        return CaptionSourceDisplayMode::PreferMicrophone;
    }
    return CaptionSourceDisplayMode::CombinedChronological;
}

std::string displayName(CaptionSourceDisplayMode mode)
{
    switch (mode) {
    case CaptionSourceDisplayMode::CombinedChronological:
        return "Combined";
    case CaptionSourceDisplayMode::SystemOnly:
        return "System only";
    case CaptionSourceDisplayMode::MicrophoneOnly:
        return "Mic only";
    case CaptionSourceDisplayMode::PreferSystemAudio:
        return "Prefer system";
    case CaptionSourceDisplayMode::PreferMicrophone:
        return "Prefer mic";
    }
    return "Combined";
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

    const auto selectedSegments = selectSegments(state, segments);
    std::vector<std::string> selectedText;
    int selectedLines = 0;
    for (auto iterator = selectedSegments.rbegin(); iterator != selectedSegments.rend(); ++iterator) {
        const std::string segmentText = formatSegment(state, *iterator);
        if (segmentText.empty()) {
            continue;
        }
        const int segmentLines = lineCount(segmentText);
        if (state.maxLines > 0 && selectedLines > 0 && selectedLines + segmentLines > state.maxLines) {
            break;
        }
        selectedText.push_back(segmentText);
        selectedLines += segmentLines;
        if (state.maxLines > 0) {
            if (selectedLines >= state.maxLines) {
                break;
            }
        }
    }

    std::reverse(selectedText.begin(), selectedText.end());
    std::string output;
    for (const auto &text : selectedText) {
        if (!output.empty()) {
            output += '\n';
        }
        output += text;
    }
    return limitOutput(output, state.maxLines, state.maxCharacters);
}

std::vector<CaptionSegment> CaptionFormatter::selectSegments(const CaptionState &state, const std::vector<CaptionSegment> &segments) const
{
    auto hasSource = [&segments](CaptionSource source) {
        return std::any_of(segments.begin(), segments.end(), [source](const CaptionSegment &segment) {
            return segment.source == source;
        });
    };

    std::vector<CaptionSegment> selected;
    selected.reserve(segments.size());
    CaptionSource requiredSource = CaptionSource::Unknown;
    bool filterBySource = false;
    switch (state.sourceDisplayMode) {
    case CaptionSourceDisplayMode::SystemOnly:
        requiredSource = CaptionSource::SystemAudio;
        filterBySource = true;
        break;
    case CaptionSourceDisplayMode::MicrophoneOnly:
        requiredSource = CaptionSource::Microphone;
        filterBySource = true;
        break;
    case CaptionSourceDisplayMode::PreferSystemAudio:
        if (hasSource(CaptionSource::SystemAudio)) {
            requiredSource = CaptionSource::SystemAudio;
            filterBySource = true;
        }
        break;
    case CaptionSourceDisplayMode::PreferMicrophone:
        if (hasSource(CaptionSource::Microphone)) {
            requiredSource = CaptionSource::Microphone;
            filterBySource = true;
        }
        break;
    case CaptionSourceDisplayMode::CombinedChronological:
        break;
    }

    for (const auto &segment : segments) {
        if (filterBySource && segment.source != requiredSource) {
            continue;
        }
        selected.push_back(segment);
    }

    std::stable_sort(selected.begin(), selected.end(), [](const CaptionSegment &left, const CaptionSegment &right) {
        if (left.startMs != right.startMs) {
            return left.startMs < right.startMs;
        }
        return left.endMs < right.endMs;
    });
    return selected;
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
        const std::string sourceText = sourcePrefix(state, segment);
        const std::string prefixText = !sourceText.empty() ? sourceText : targetLanguagePrefix(state);
        return text.empty() ? std::string {} : prefixText + text;
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
        const std::string sourceText = sourcePrefix(state, segment);
        const std::string prefixText = !sourceText.empty() ? sourceText : std::string("Key point: ");
        return text.empty() ? std::string {} : prefixText + text;
    }
    }
    return {};
}

std::string CaptionFormatter::speakerPrefix(const CaptionState &state, const CaptionSegment &segment) const
{
    const std::string source = sourcePrefix(state, segment);
    if (!source.empty()) {
        return source;
    }
    if (!state.showSpeaker || segment.speaker.empty()) {
        return {};
    }
    return segment.speaker + ": ";
}

std::string CaptionFormatter::sourcePrefix(const CaptionState &state, const CaptionSegment &segment) const
{
    if (!state.showSourceLabels) {
        return {};
    }
    const std::string label = labelForSource(segment.source);
    return label.empty() ? std::string {} : label + ": ";
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

int CaptionFormatter::lineCount(const std::string &text) const
{
    if (text.empty()) {
        return 0;
    }
    return static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;
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
