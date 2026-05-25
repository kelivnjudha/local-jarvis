#pragma once

#include "AsrEngine.h"

#include <mutex>

struct whisper_context;

namespace local_jarvis::asr {

class WhisperAsrEngine final : public AsrEngine {
public:
    ~WhisperAsrEngine() override;

    void configure(const AsrEngineConfig &config) override;
    bool initialize(const std::string &modelPath) override;
    AsrResult transcribeChunk(const AsrInputChunk &chunk) override;
    void shutdown() override;

    [[nodiscard]] std::string engineName() const override;
    [[nodiscard]] bool isInitialized() const override;
    [[nodiscard]] bool isReady() const override;
    [[nodiscard]] std::string lastError() const override;

private:
    [[nodiscard]] std::string effectiveModelPath(const std::string &modelPath) const;
    void setLastError(const std::string &error);

    mutable std::mutex m_mutex;
    AsrEngineConfig m_config;
    whisper_context *m_context = nullptr;
    bool m_initialized = false;
    std::string m_modelPath;
    std::string m_lastError;
};

} // namespace local_jarvis::asr
