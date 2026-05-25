#pragma once

#include "ProcessingJob.h"
#include "ai/OllamaClient.h"
#include "storage/Storage.h"

#include <string>

namespace local_jarvis::processing {

class StudyNoteProcessor {
public:
    StudyNoteProcessor(storage::Storage &storage, ai::OllamaClient &ollamaClient);

    ProcessingResult processChunk(const std::string &sessionId, const std::string &modelName);
    ProcessingResult processFinalSummary(const std::string &sessionId, const std::string &modelName);

private:
    [[nodiscard]] std::string buildTranscriptContext(const std::string &sessionId) const;
    [[nodiscard]] std::string buildScreenOcrContext(const std::string &sessionId) const;
    ProcessingResult persistStudyOutput(
        const std::string &sessionId,
        const std::string &modelName,
        const std::string &noteType,
        const std::string &rawOutput);

    storage::Storage &m_storage;
    ai::OllamaClient &m_ollamaClient;
};

} // namespace local_jarvis::processing
