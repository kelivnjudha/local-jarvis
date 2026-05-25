#pragma once

#include <cstdint>
#include <functional>
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
    double lastBufferPeak = 0.0;
    double lastBufferDbfs = -120.0;
    double lastBufferNonZeroRatio = 0.0;
    double smoothedLevel = 0.0;
    std::int64_t lastCallbackTimeMs = 0;
    std::string lastError;
};

// Captured PCM stays in memory only. Windows WASAPI packets are converted to
// normalized float samples for metering and optional local ASR, then dropped.
struct PcmAudioFrame {
    int sampleRate = 0;
    int channelCount = 0;
    std::vector<float> samples;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    std::int64_t timestampMs = 0;
};

using PcmAudioCallback = std::function<void(const PcmAudioFrame &)>;

} // namespace local_jarvis::audio
