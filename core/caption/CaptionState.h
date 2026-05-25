#pragma once

#include "CaptionTypes.h"

#include <string>
#include <vector>

namespace local_jarvis::caption {

struct CaptionState {
    bool captionsEnabled = true;
    CaptionMode captionMode = CaptionMode::OriginalAndTranslation;
    std::string sourceLanguage = "auto";
    std::string targetLanguage = "en";
    bool showSpeaker = true;
    int maxLines = 2;
    int maxCharacters = 240;
    std::vector<CaptionSegment> latestSegments;
    std::string lastUpdatedAt;
};

} // namespace local_jarvis::caption
