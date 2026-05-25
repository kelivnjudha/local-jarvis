#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace local_jarvis::audio {

class AudioLevelMeter {
public:
    void processSamples(std::span<const float> samples);
    void processInterleavedSamples(std::span<const float> samples, int channelCount);
    void processInt16Samples(std::span<const std::int16_t> samples);
    void reset();

    [[nodiscard]] double level() const;
    [[nodiscard]] double lastRms() const;

    [[nodiscard]] static double calculateRms(std::span<const float> samples);
    [[nodiscard]] static std::vector<float> mixInterleavedToMono(std::span<const float> samples, int channelCount);

private:
    void smoothTo(double targetLevel);

    double m_level = 0.0;
    double m_lastRms = 0.0;
};

} // namespace local_jarvis::audio
