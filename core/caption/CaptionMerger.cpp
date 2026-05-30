#include "CaptionMerger.h"

#include <algorithm>

namespace local_jarvis::caption {

bool CaptionMerger::canMerge(
    const CaptionSegment &previous,
    const CaptionSegment &next,
    const CaptionMergerOptions &options) const
{
    if (!options.enabled) {
        return false;
    }
    if (!options.allowCrossSource && previous.source != next.source) {
        return false;
    }
    if (!previous.speaker.empty() && !next.speaker.empty() && previous.speaker != next.speaker) {
        return false;
    }
    if (next.endMs < previous.endMs) {
        return false;
    }

    const auto gapMs = std::max<std::int64_t>(0, next.startMs - previous.endMs);
    if (gapMs > options.maxGapMs) {
        return false;
    }
    return static_cast<int>(mergedTextLength(previous, next)) <= options.maxCharacters;
}

CaptionSegment CaptionMerger::merge(const CaptionSegment &previous, const CaptionSegment &next) const
{
    CaptionSegment merged = previous;
    if (!next.id.empty()) {
        merged.id = previous.id.empty() ? next.id : previous.id + "+" + next.id;
    }
    if (merged.speaker.empty()) {
        merged.speaker = next.speaker;
    }
    if (merged.detectedLanguage.empty()) {
        merged.detectedLanguage = next.detectedLanguage;
    }
    merged.originalText = mergeText(previous.originalText, next.originalText);
    merged.translatedText = mergeText(previous.translatedText, next.translatedText);
    merged.summaryText = mergeText(previous.summaryText, next.summaryText);
    merged.startMs = std::min(previous.startMs, next.startMs);
    merged.endMs = std::max(previous.endMs, next.endMs);
    merged.isFinal = previous.isFinal && next.isFinal;
    return merged;
}

bool CaptionMerger::mergeIntoLatest(
    std::vector<CaptionSegment> &segments,
    const CaptionSegment &next,
    const CaptionMergerOptions &options) const
{
    if (segments.empty() || !canMerge(segments.back(), next, options)) {
        return false;
    }
    segments.back() = merge(segments.back(), next);
    return true;
}

std::string CaptionMerger::mergeText(const std::string &previous, const std::string &next) const
{
    if (previous.empty()) {
        return next;
    }
    if (next.empty()) {
        return previous;
    }
    return previous + ' ' + next;
}

std::size_t CaptionMerger::mergedTextLength(const CaptionSegment &previous, const CaptionSegment &next) const
{
    const auto originalLength = mergeText(previous.originalText, next.originalText).size();
    const auto translatedLength = mergeText(previous.translatedText, next.translatedText).size();
    const auto summaryLength = mergeText(previous.summaryText, next.summaryText).size();
    return std::max({ originalLength, translatedLength, summaryLength });
}

} // namespace local_jarvis::caption
