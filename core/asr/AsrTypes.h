#pragma once

#include "caption/CaptionTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace local_jarvis::asr {

enum class AsrStatus {
    Disabled,
    Ready,
    Listening,
    Processing,
    Error
};

struct AsrInputChunk {
    std::uint64_t chunkId = 0;
    std::string sessionId;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    int sampleRate = 16000;
    int channels = 1;
    std::vector<float> samples;
    bool isFinalChunk = false;
};

struct AsrTranscriptSegment {
    std::string id;
    std::string sessionId;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    std::string speaker = "Microphone";
    std::string text;
    std::string detectedLanguage = "en";
    double confidence = 0.0;
    bool isFinal = true;
};

struct PcmAudioBuffer {
    std::vector<float> samples;
    int sampleRateHz = 16000;
    int channelCount = 1;
};

struct AsrResult {
    bool ok = false;
    AsrTranscriptSegment segment;
    std::string text;
    std::string message;
};

[[nodiscard]] inline std::string toString(AsrStatus status)
{
    switch (status) {
    case AsrStatus::Disabled:
        return "Disabled";
    case AsrStatus::Ready:
        return "Ready";
    case AsrStatus::Listening:
        return "Listening";
    case AsrStatus::Processing:
        return "Processing";
    case AsrStatus::Error:
        return "Error";
    }
    return "Unknown";
}

[[nodiscard]] inline caption::CaptionSegment toCaptionSegment(const AsrTranscriptSegment &segment)
{
    return caption::CaptionSegment {
        .id = segment.id,
        .speaker = segment.speaker,
        .originalText = segment.text,
        .translatedText = segment.detectedLanguage == "en" ? segment.text : std::string {},
        .summaryText = segment.text,
        .detectedLanguage = segment.detectedLanguage,
        .startMs = segment.startMs,
        .endMs = segment.endMs,
        .isFinal = segment.isFinal
    };
}

} // namespace local_jarvis::asr
