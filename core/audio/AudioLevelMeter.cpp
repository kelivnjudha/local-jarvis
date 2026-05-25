#include "AudioLevelMeter.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace local_jarvis::audio {

void AudioLevelMeter::processSamples(std::span<const float> samples)
{
    if (samples.empty()) {
        smoothTo(0.0);
        return;
    }

    double sumSquares = 0.0;
    for (float sample : samples) {
        const double clamped = std::clamp(static_cast<double>(sample), -1.0, 1.0);
        sumSquares += clamped * clamped;
    }

    const double rms = std::sqrt(sumSquares / static_cast<double>(samples.size()));
    smoothTo(std::clamp(rms, 0.0, 1.0));
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
}

double AudioLevelMeter::level() const
{
    return m_level;
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
