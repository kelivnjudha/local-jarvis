#pragma once

#include <string>
#include <vector>

namespace local_jarvis::asr {

struct PcmAudioBuffer {
    std::vector<float> samples;
    int sampleRateHz = 16000;
    int channelCount = 1;
};

struct AsrResult {
    bool ok = false;
    std::string text;
    std::string message;
};

class AsrEngine {
public:
    virtual ~AsrEngine() = default;

    virtual bool initialize(const std::string &modelPath) = 0;
    virtual AsrResult transcribePcm(const PcmAudioBuffer &audioBuffer) = 0;
    virtual void shutdown() = 0;

    [[nodiscard]] virtual std::string engineName() const = 0;
    [[nodiscard]] virtual bool isInitialized() const = 0;
};

} // namespace local_jarvis::asr
