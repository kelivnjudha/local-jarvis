#pragma once

#include <string>

namespace local_jarvis::processing {

enum class ProcessingJobType {
    StudyChunk,
    MeetingChunk,
    FinalSessionSummary
};

struct ProcessingJob {
    std::string id;
    ProcessingJobType type = ProcessingJobType::StudyChunk;
    std::string sessionId;

    [[nodiscard]] std::string typeName() const;

    static ProcessingJob studyChunk(const std::string &sessionId);
    static ProcessingJob meetingChunk(const std::string &sessionId);
    static ProcessingJob finalSessionSummary(const std::string &sessionId);
};

struct ProcessingResult {
    bool ok = false;
    std::string message;
    std::string rawOutput;
};

} // namespace local_jarvis::processing
