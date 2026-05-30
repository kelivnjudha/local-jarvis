#include "WhisperAudioConversion.h"

#include "audio/AudioLevelMeter.h"

#include <algorithm>
#include <cmath>

namespace local_jarvis::asr {

std::vector<float> resampleToWhisperRate(
    std::span<const float> monoSamples,
    int inputSampleRate,
    int outputSampleRate)
{
    if (monoSamples.empty() || inputSampleRate <= 0 || outputSampleRate <= 0) {
        return {};
    }

    if (inputSampleRate == outputSampleRate) {
        std::vector<float> copied(monoSamples.begin(), monoSamples.end());
        for (auto &sample : copied) {
            sample = std::clamp(sample, -1.0F, 1.0F);
        }
        return copied;
    }

    // MVP resampler: linear interpolation is good enough for local scaffold
    // verification. Replace with a higher-quality resampler before ASR quality
    // work depends on this path.
    const double ratio = static_cast<double>(outputSampleRate) / static_cast<double>(inputSampleRate);
    const auto outputCount = static_cast<std::size_t>(
        std::max(1.0, std::round(static_cast<double>(monoSamples.size()) * ratio)));

    std::vector<float> output;
    output.reserve(outputCount);
    for (std::size_t index = 0; index < outputCount; ++index) {
        const double sourcePosition = static_cast<double>(index) / ratio;
        const auto lower = static_cast<std::size_t>(std::floor(sourcePosition));
        const auto upper = std::min<std::size_t>(lower + 1, monoSamples.size() - 1);
        const double fraction = sourcePosition - static_cast<double>(lower);
        const float sample = static_cast<float>(
            (static_cast<double>(monoSamples[lower]) * (1.0 - fraction))
            + (static_cast<double>(monoSamples[upper]) * fraction));
        output.push_back(std::clamp(sample, -1.0F, 1.0F));
    }
    return output;
}

std::vector<float> prepareWhisperSamples(const AsrInputChunk &chunk)
{
    const int channels = std::max(1, chunk.channels);
    auto monoSamples = audio::AudioLevelMeter::mixInterleavedToMono(chunk.samples, channels);
    return resampleToWhisperRate(monoSamples, chunk.sampleRate, kWhisperSampleRate);
}

double normalizedRms(std::span<const float> samples)
{
    return audio::AudioLevelMeter::calculateRms(samples);
}

bool isProbablySilent(std::span<const float> samples, double threshold)
{
    if (samples.empty()) {
        return true;
    }
    return normalizedRms(samples) <= threshold;
}

WhisperPreprocessingResult preprocessWhisperSamples(
    std::span<const float> samples,
    const AsrPreprocessingConfig &config)
{
    WhisperPreprocessingResult result;
    result.samples.assign(samples.begin(), samples.end());
    if (!config.enabled || result.samples.empty()) {
        return result;
    }

    const double currentRms = normalizedRms(result.samples);
    if (currentRms <= 0.000001) {
        return result;
    }

    const double targetRms = std::clamp(config.targetRms, 0.005, 0.35);
    const double maxGainDb = std::clamp(config.maxGainDb, 0.0, 30.0);
    const double requestedGain = targetRms / currentRms;
    const double maxGain = std::pow(10.0, maxGainDb / 20.0);
    const double gain = std::clamp(requestedGain, 1.0, maxGain);

    if (gain <= 1.0001) {
        return result;
    }

    result.appliedGainDb = 20.0 * std::log10(gain);
    for (float &sample : result.samples) {
        const double amplified = static_cast<double>(sample) * gain;
        const double limited = std::clamp(amplified, -0.98, 0.98);
        if (limited != amplified) {
            result.limiterEngaged = true;
        }
        sample = static_cast<float>(limited);
    }
    return result;
}

} // namespace local_jarvis::asr
