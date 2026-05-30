#pragma once

#include <string>

namespace local_jarvis::audio {

enum class SpeechDetectionState {
    Silence,
    TooQuiet,
    MaybeSpeech,
    SpeechLikely,
    ClippingRisk
};

struct AudioSpeechDetectorConfig {
    double silenceDbfsThreshold = -60.0;
    double tooQuietDbfsThreshold = -45.0;
    double speechLikelyDbfsThreshold = -32.0;
    double clippingPeakThreshold = 0.90;
    double minNonZeroPercentage = 0.02;
    int minChunkDurationMs = 300;
};

struct AudioSpeechFeatures {
    double rms = 0.0;
    double peak = 0.0;
    double dbfs = -120.0;
    double nonZeroPercentage = 0.0;
    int chunkDurationMs = 0;
    double smoothedLevel = -1.0;
};

class AudioSpeechDetector {
public:
    explicit AudioSpeechDetector(AudioSpeechDetectorConfig config = {});

    [[nodiscard]] SpeechDetectionState classify(const AudioSpeechFeatures &features) const;
    [[nodiscard]] const AudioSpeechDetectorConfig &config() const;

private:
    AudioSpeechDetectorConfig m_config;
};

[[nodiscard]] std::string toString(SpeechDetectionState state);
[[nodiscard]] std::string displayName(SpeechDetectionState state);

} // namespace local_jarvis::audio
