#include "asr/WhisperAudioConversion.h"
#include "audio/AudioSpeechDetector.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

std::vector<float> samples(std::size_t count, float value)
{
    return std::vector<float>(count, value);
}

} // namespace

int main()
{
    using local_jarvis::audio::AudioSpeechDetector;
    using local_jarvis::audio::AudioSpeechFeatures;
    using local_jarvis::audio::SpeechDetectionState;

    AudioSpeechDetector detector;
    if (!expect(detector.classify(AudioSpeechFeatures {
            .rms = 0.0,
            .peak = 0.0,
            .dbfs = -120.0,
            .nonZeroPercentage = 0.0,
            .chunkDurationMs = 3000 }) == SpeechDetectionState::Silence,
            "Zero audio should classify as silence.")) {
        return EXIT_FAILURE;
    }
    if (!expect(detector.classify(AudioSpeechFeatures {
            .rms = 0.002,
            .peak = 0.006,
            .dbfs = -54.0,
            .nonZeroPercentage = 0.80,
            .chunkDurationMs = 3000 }) == SpeechDetectionState::TooQuiet,
            "Low non-silent audio should classify as too quiet.")) {
        return EXIT_FAILURE;
    }
    if (!expect(detector.classify(AudioSpeechFeatures {
            .rms = 0.02,
            .peak = 0.05,
            .dbfs = -34.0,
            .nonZeroPercentage = 0.90,
            .chunkDurationMs = 3000 }) == SpeechDetectionState::MaybeSpeech,
            "Mid-level audio should classify as maybe speech.")) {
        return EXIT_FAILURE;
    }
    if (!expect(detector.classify(AudioSpeechFeatures {
            .rms = 0.09,
            .peak = 0.18,
            .dbfs = -21.0,
            .nonZeroPercentage = 0.95,
            .chunkDurationMs = 3000 }) == SpeechDetectionState::SpeechLikely,
            "Clear audio should classify as likely speech.")) {
        return EXIT_FAILURE;
    }
    if (!expect(detector.classify(AudioSpeechFeatures {
            .rms = 0.40,
            .peak = 0.96,
            .dbfs = -8.0,
            .nonZeroPercentage = 0.95,
            .chunkDurationMs = 3000 }) == SpeechDetectionState::ClippingRisk,
            "High peaks should classify as clipping risk.")) {
        return EXIT_FAILURE;
    }

    const auto quiet = samples(16000, 0.01F);
    const auto quietRms = local_jarvis::asr::normalizedRms(quiet);
    const auto boosted = local_jarvis::asr::preprocessWhisperSamples(quiet, local_jarvis::asr::AsrPreprocessingConfig {
        .enabled = true,
        .targetRms = 0.08,
        .maxGainDb = 6.0
    });
    if (!expect(boosted.appliedGainDb > 0.0 && boosted.appliedGainDb <= 6.1,
            "Preprocessing gain should be applied and capped.")) {
        return EXIT_FAILURE;
    }
    if (!expect(local_jarvis::asr::normalizedRms(boosted.samples) > quietRms,
            "Preprocessing should raise low-but-valid RMS.")) {
        return EXIT_FAILURE;
    }

    auto peaky = samples(64, 0.01F);
    peaky.front() = 0.8F;
    const auto limited = local_jarvis::asr::preprocessWhisperSamples(peaky, local_jarvis::asr::AsrPreprocessingConfig {
        .enabled = true,
        .targetRms = 0.35,
        .maxGainDb = 30.0
    });
    return expect(limited.limiterEngaged, "Preprocessing should engage limiter when gain would clip.")
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
