#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace local_jarvis::audio {

struct AudioInputDevice {
    std::string id;
    std::string displayName;
    bool isDefault = false;
    bool isAvailable = true;
};

// Phase 3A keeps captured PCM frames in memory only. Windows WASAPI packets are
// converted to normalized interleaved float samples for metering, then dropped.
struct PcmAudioFrame {
    int sampleRate = 0;
    int channelCount = 0;
    std::vector<float> samples;
    std::int64_t timestampMs = 0;
};

} // namespace local_jarvis::audio
