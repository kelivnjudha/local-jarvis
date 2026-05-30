#pragma once

#include "CaptionTypes.h"

#include <vector>

namespace local_jarvis::caption {

struct CaptionMergerOptions {
    bool enabled = true;
    bool allowCrossSource = false;
    int maxGapMs = 1200;
    int maxCharacters = 220;
};

class CaptionMerger {
public:
    [[nodiscard]] bool canMerge(
        const CaptionSegment &previous,
        const CaptionSegment &next,
        const CaptionMergerOptions &options = {}) const;
    [[nodiscard]] CaptionSegment merge(
        const CaptionSegment &previous,
        const CaptionSegment &next) const;
    bool mergeIntoLatest(
        std::vector<CaptionSegment> &segments,
        const CaptionSegment &next,
        const CaptionMergerOptions &options = {}) const;

private:
    [[nodiscard]] std::string mergeText(const std::string &previous, const std::string &next) const;
    [[nodiscard]] std::size_t mergedTextLength(const CaptionSegment &previous, const CaptionSegment &next) const;
};

} // namespace local_jarvis::caption
