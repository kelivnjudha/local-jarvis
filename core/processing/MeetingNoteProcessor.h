#pragma once

#include "ProcessingJob.h"
#include "ai/OllamaClient.h"
#include "storage/Storage.h"

#include <string>

namespace local_jarvis::processing {

class MeetingNoteProcessor {
public:
    MeetingNoteProcessor(storage::Storage &storage, ai::OllamaClient &ollamaClient);

    ProcessingResult processChunk(const std::string &sessionId, const std::string &modelName);

private:
    [[nodiscard]] std::string buildTranscriptContext(const std::string &sessionId) const;
    [[nodiscard]] std::string buildScreenOcrContext(const std::string &sessionId) const;
    ProcessingResult persistMeetingOutput(const std::string &sessionId, const std::string &modelName, const std::string &rawOutput);

    storage::Storage &m_storage;
    ai::OllamaClient &m_ollamaClient;
};

} // namespace local_jarvis::processing
