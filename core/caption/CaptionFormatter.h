#pragma once

#include "CaptionState.h"

#include <string>
#include <vector>

namespace local_jarvis::caption {

class CaptionFormatter {
public:
    [[nodiscard]] std::string format(const CaptionState &state) const;
    [[nodiscard]] std::string format(const CaptionState &state, const std::vector<CaptionSegment> &segments) const;

private:
    [[nodiscard]] std::string formatSegment(const CaptionState &state, const CaptionSegment &segment) const;
    [[nodiscard]] std::string speakerPrefix(const CaptionState &state, const CaptionSegment &segment) const;
    [[nodiscard]] std::string targetLanguagePrefix(const CaptionState &state) const;
    [[nodiscard]] std::string limitOutput(const std::string &text, int maxLines, int maxCharacters) const;
    [[nodiscard]] std::string truncateUtf8(const std::string &text, int maxCharacters) const;
};

} // namespace local_jarvis::caption
