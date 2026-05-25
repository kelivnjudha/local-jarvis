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

struct MicrophoneDiagnostics {
    std::string selectedDeviceName;
    std::string selectedDeviceId;
    bool captureActive = false;
    int sampleRate = 0;
    int channelCount = 0;
    std::string sampleFormat;
    std::uint64_t buffersReceived = 0;
    std::uint64_t framesReceived = 0;
    std::uint64_t nonZeroSamplesObserved = 0;
    double lastBufferRms = 0.0;
    double smoothedLevel = 0.0;
    std::int64_t lastCallbackTimeMs = 0;
    std::string lastError;
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
