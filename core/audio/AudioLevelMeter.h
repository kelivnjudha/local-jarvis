#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace local_jarvis::audio {

class AudioLevelMeter {
public:
    void processSamples(std::span<const float> samples);
    void processInt16Samples(std::span<const std::int16_t> samples);
    void reset();

    [[nodiscard]] double level() const;

private:
    void smoothTo(double targetLevel);

    double m_level = 0.0;
};

} // namespace local_jarvis::audio
