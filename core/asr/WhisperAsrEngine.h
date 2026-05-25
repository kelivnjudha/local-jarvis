#pragma once

#include "AsrEngine.h"

namespace local_jarvis::asr {

class WhisperAsrEngine final : public AsrEngine {
public:
    bool initialize(const std::string &modelPath) override;
    AsrResult transcribeChunk(const AsrInputChunk &chunk) override;
    void shutdown() override;

    [[nodiscard]] std::string engineName() const override;
    [[nodiscard]] bool isInitialized() const override;

private:
    bool m_initialized = false;
    std::string m_modelPath;
};

} // namespace local_jarvis::asr
