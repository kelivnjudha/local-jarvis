#pragma once

#include "AsrEngine.h"

namespace local_jarvis::asr {

class StubAsrEngine final : public AsrEngine {
public:
    bool initialize(const std::string &modelPath) override;
    AsrResult transcribeChunk(const AsrInputChunk &chunk) override;
    void shutdown() override;

    [[nodiscard]] std::string engineName() const override;
    [[nodiscard]] bool isInitialized() const override;

private:
    bool m_initialized = false;
    std::uint64_t m_nextChunkNumber = 1;
};

} // namespace local_jarvis::asr
