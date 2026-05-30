#pragma once

#include "audio/AudioSpeechDetector.h"
#include "caption/CaptionTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace local_jarvis::asr {

enum class AsrStatus {
    Disabled,
    Loading,
    Ready,
    Listening,
    Processing,
    Error
};

enum class AsrBackend {
    Stub,
    Whisper
};

enum class AsrAudioSource {
    Microphone,
    SystemAudio
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
    AsrAudioSource audioSource = AsrAudioSource::Microphone;
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
    AsrAudioSource audioSource = AsrAudioSource::Microphone;
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

struct AsrPreprocessingConfig {
    bool enabled = false;
    double targetRms = 0.08;
    double maxGainDb = 12.0;
};

struct AsrEngineConfig {
    std::string modelPath;
    std::string language = "auto";
    bool translateToEnglish = false;
    int maxThreads = 4;
    audio::AudioSpeechDetectorConfig speechDetection {};
    bool skipTooQuietChunks = true;
    bool debugProcessTooQuiet = false;
    AsrPreprocessingConfig preprocessing {};
};

[[nodiscard]] inline std::string toString(AsrBackend backend)
{
    switch (backend) {
    case AsrBackend::Stub:
        return "stub";
    case AsrBackend::Whisper:
        return "whisper";
    }
    return "stub";
}

[[nodiscard]] inline std::string toString(AsrAudioSource source)
{
    switch (source) {
    case AsrAudioSource::Microphone:
        return "microphone";
    case AsrAudioSource::SystemAudio:
        return "system_audio";
    }
    return "microphone";
}

[[nodiscard]] inline std::string displayName(AsrAudioSource source)
{
    switch (source) {
    case AsrAudioSource::Microphone:
        return "Microphone";
    case AsrAudioSource::SystemAudio:
        return "System audio";
    }
    return "Microphone";
}

[[nodiscard]] inline std::string captionSpeakerForSource(AsrAudioSource source)
{
    switch (source) {
    case AsrAudioSource::Microphone:
        return "Mic";
    case AsrAudioSource::SystemAudio:
        return "System";
    }
    return "Mic";
}

[[nodiscard]] inline std::string transcriptSourceFor(AsrBackend backend, AsrAudioSource source)
{
    if (source == AsrAudioSource::SystemAudio) {
        return backend == AsrBackend::Whisper
            ? "system_audio_asr_whisper"
            : "system_audio_asr_stub";
    }
    return backend == AsrBackend::Whisper
        ? "microphone_asr_whisper"
        : "microphone_asr_stub";
}

[[nodiscard]] inline AsrBackend asrBackendFromString(const std::string &value)
{
    if (value == "whisper" || value == "Whisper") {
        return AsrBackend::Whisper;
    }
    return AsrBackend::Stub;
}

[[nodiscard]] inline std::string displayName(AsrBackend backend)
{
    switch (backend) {
    case AsrBackend::Stub:
        return "Stub";
    case AsrBackend::Whisper:
        return "Whisper";
    }
    return "Stub";
}

[[nodiscard]] inline std::string toString(AsrStatus status)
{
    switch (status) {
    case AsrStatus::Disabled:
        return "Disabled";
    case AsrStatus::Loading:
        return "Loading";
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
        .speaker = segment.speaker.empty() ? captionSpeakerForSource(segment.audioSource) : segment.speaker,
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
