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
    [[nodiscard]] std::vector<CaptionSegment> selectSegments(const CaptionState &state, const std::vector<CaptionSegment> &segments) const;
    [[nodiscard]] std::string formatSegment(const CaptionState &state, const CaptionSegment &segment) const;
    [[nodiscard]] std::string speakerPrefix(const CaptionState &state, const CaptionSegment &segment) const;
    [[nodiscard]] std::string sourcePrefix(const CaptionState &state, const CaptionSegment &segment) const;
    [[nodiscard]] std::string targetLanguagePrefix(const CaptionState &state) const;
    [[nodiscard]] int lineCount(const std::string &text) const;
    [[nodiscard]] int targetLineLength(const CaptionState &state) const;
    [[nodiscard]] std::string wrapOutputLines(const CaptionState &state, const std::string &text) const;
    [[nodiscard]] std::string wrapLine(const std::string &line, int targetLength) const;
    [[nodiscard]] std::string limitOutput(const std::string &text, int maxLines, int maxCharacters) const;
    [[nodiscard]] std::string truncateUtf8(const std::string &text, int maxCharacters) const;
};

} // namespace local_jarvis::caption
