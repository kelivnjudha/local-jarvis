#include "AudioLevelMeter.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace local_jarvis::audio {

void AudioLevelMeter::processSamples(std::span<const float> samples)
{
    if (samples.empty()) {
        m_lastRms = 0.0;
        smoothTo(0.0);
        return;
    }

    m_lastRms = calculateRms(samples);
    smoothTo(m_lastRms);
}

void AudioLevelMeter::processInterleavedSamples(std::span<const float> samples, int channelCount)
{
    const auto mono = mixInterleavedToMono(samples, channelCount);
    processSamples(mono);
}

void AudioLevelMeter::processInt16Samples(std::span<const std::int16_t> samples)
{
    std::vector<float> normalized;
    normalized.reserve(samples.size());
    for (std::int16_t sample : samples) {
        normalized.push_back(static_cast<float>(sample) / 32768.0F);
    }
    processSamples(normalized);
}

void AudioLevelMeter::reset()
{
    m_level = 0.0;
    m_lastRms = 0.0;
}

double AudioLevelMeter::level() const
{
    return m_level;
}

double AudioLevelMeter::lastRms() const
{
    return m_lastRms;
}

double AudioLevelMeter::calculateRms(std::span<const float> samples)
{
    if (samples.empty()) {
        return 0.0;
    }

    double sumSquares = 0.0;
    for (float sample : samples) {
        const double clamped = std::clamp(static_cast<double>(sample), -1.0, 1.0);
        sumSquares += clamped * clamped;
    }

    return std::clamp(std::sqrt(sumSquares / static_cast<double>(samples.size())), 0.0, 1.0);
}

std::vector<float> AudioLevelMeter::mixInterleavedToMono(std::span<const float> samples, int channelCount)
{
    if (samples.empty()) {
        return {};
    }

    const int channels = std::max(channelCount, 1);
    if (channels == 1) {
        return { samples.begin(), samples.end() };
    }

    const std::size_t frameCount = samples.size() / static_cast<std::size_t>(channels);
    std::vector<float> mono;
    mono.reserve(frameCount);
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        double sum = 0.0;
        const std::size_t frameOffset = frame * static_cast<std::size_t>(channels);
        for (int channel = 0; channel < channels; ++channel) {
            sum += std::clamp(static_cast<double>(samples[frameOffset + static_cast<std::size_t>(channel)]), -1.0, 1.0);
        }
        mono.push_back(static_cast<float>(sum / static_cast<double>(channels)));
    }
    return mono;
}

void AudioLevelMeter::smoothTo(double targetLevel)
{
    const double alpha = targetLevel > m_level ? 0.55 : 0.25;
    m_level = std::clamp((m_level * (1.0 - alpha)) + (targetLevel * alpha), 0.0, 1.0);
    if (m_level < 0.0001) {
        m_level = 0.0;
    }
}

} // namespace local_jarvis::audio
