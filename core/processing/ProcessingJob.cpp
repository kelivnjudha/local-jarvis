#include "ProcessingJob.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace local_jarvis::processing {
namespace {

std::string generateJobId()
{
    static thread_local std::mt19937_64 rng { std::random_device {}() };
    std::uniform_int_distribution<unsigned long long> distribution;

    std::ostringstream stream;
    stream << "processing-job-"
           << std::hex << std::setw(16) << std::setfill('0') << distribution(rng)
           << std::setw(16) << std::setfill('0') << distribution(rng);
    return stream.str();
}

ProcessingJob makeJob(ProcessingJobType type, const std::string &sessionId)
{
    return ProcessingJob {
        .id = generateJobId(),
        .type = type,
        .sessionId = sessionId
    };
}

} // namespace

std::string ProcessingJob::typeName() const
{
    switch (type) {
    case ProcessingJobType::StudyChunk:
        return "study_chunk";
    case ProcessingJobType::MeetingChunk:
        return "meeting_chunk";
    case ProcessingJobType::FinalSessionSummary:
        return "final_session_summary";
    }

    return "unknown";
}

ProcessingJob ProcessingJob::studyChunk(const std::string &sessionId)
{
    return makeJob(ProcessingJobType::StudyChunk, sessionId);
}

ProcessingJob ProcessingJob::meetingChunk(const std::string &sessionId)
{
    return makeJob(ProcessingJobType::MeetingChunk, sessionId);
}

ProcessingJob ProcessingJob::finalSessionSummary(const std::string &sessionId)
{
    return makeJob(ProcessingJobType::FinalSessionSummary, sessionId);
}

} // namespace local_jarvis::processing
