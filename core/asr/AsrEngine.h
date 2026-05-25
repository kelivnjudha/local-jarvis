#pragma once

#include "AsrTypes.h"

#include <string>

namespace local_jarvis::asr {

class AsrEngine {
public:
    virtual ~AsrEngine() = default;

    virtual void configure(const AsrEngineConfig &config);
    virtual bool initialize(const std::string &modelPath) = 0;
    virtual AsrResult transcribeChunk(const AsrInputChunk &chunk) = 0;
    virtual AsrResult transcribePcm(const PcmAudioBuffer &audioBuffer);
    virtual void shutdown() = 0;

    [[nodiscard]] virtual std::string engineName() const = 0;
    [[nodiscard]] virtual bool isInitialized() const = 0;
    [[nodiscard]] virtual bool isReady() const;
    [[nodiscard]] virtual std::string lastError() const;
};

} // namespace local_jarvis::asr
