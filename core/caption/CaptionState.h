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
    bool showSourceLabels = true;
    CaptionSourceDisplayMode sourceDisplayMode = CaptionSourceDisplayMode::CombinedChronological;
    int maxLines = 2;
    int maxCharacters = 240;
    int holdMs = 4000;
    bool suppressDuplicates = true;
    int duplicateWindowMs = 5000;
    bool clearOnAsrOff = false;
    std::vector<CaptionSegment> latestSegments;
    std::string lastUpdatedAt;
};

} // namespace local_jarvis::caption
