#include "ProcessingQueue.h"

#include "ai/AiTypes.h"

#include <utility>

namespace local_jarvis::processing {

ProcessingQueue::ProcessingQueue(storage::Storage &storage, ai::ModelClient &modelClient)
    : m_storage(storage)
    , m_modelClient(modelClient)
    , m_studyProcessor(storage, modelClient)
    , m_meetingProcessor(storage, modelClient)
{
}

ProcessingQueue::~ProcessingQueue()
{
    stop();
}

void ProcessingQueue::start()
{
    std::lock_guard lock(m_mutex);
    if (m_worker.joinable()) {
        return;
    }

    m_stopRequested = false;
    m_worker = std::thread([this]() {
        workerLoop();
    });
}

void ProcessingQueue::stop()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = true;
    }
    m_condition.notify_all();

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void ProcessingQueue::setStatusCallback(StatusCallback callback)
{
    std::lock_guard lock(m_mutex);
    m_statusCallback = std::move(callback);
}

void ProcessingQueue::enqueueStudyChunk(const std::string &sessionId)
{
    enqueue(ProcessingJob::studyChunk(sessionId));
}

void ProcessingQueue::enqueueMeetingChunk(const std::string &sessionId)
{
    enqueue(ProcessingJob::meetingChunk(sessionId));
}

void ProcessingQueue::enqueueFinalSessionSummary(const std::string &sessionId)
{
    enqueue(ProcessingJob::finalSessionSummary(sessionId));
}

bool ProcessingQueue::isRunning() const
{
    std::lock_guard lock(m_mutex);
    return m_runningJob || !m_jobs.empty();
}

void ProcessingQueue::enqueue(ProcessingJob job)
{
    start();
    {
        std::lock_guard lock(m_mutex);
        m_jobs.push(std::move(job));
    }
    m_condition.notify_one();
}

void ProcessingQueue::workerLoop()
{
    while (true) {
        ProcessingJob job;
        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this]() {
                return m_stopRequested || !m_jobs.empty();
            });

            if (m_stopRequested && m_jobs.empty()) {
                return;
            }

            job = m_jobs.front();
            m_jobs.pop();
            m_runningJob = true;
        }

        runJob(job);

        {
            std::lock_guard lock(m_mutex);
            m_runningJob = false;
        }
    }
}

void ProcessingQueue::runJob(const ProcessingJob &job)
{
    const std::string modelName = currentModelName();
    const std::string startMessage = "AI processing job started: " + job.typeName();
    emitStatus(startMessage);
    m_storage.addModelEvent("processing_job_started", modelName, startMessage + " session=" + job.sessionId);
    if (job.type == ProcessingJobType::FinalSessionSummary) {
        m_storage.setSessionSummaryStatus(job.sessionId, "processing");
    }

    ProcessingResult result;
    switch (job.type) {
    case ProcessingJobType::StudyChunk:
        result = m_studyProcessor.processChunk(job.sessionId, modelName);
        break;
    case ProcessingJobType::MeetingChunk:
        result = m_meetingProcessor.processChunk(job.sessionId, modelName);
        break;
    case ProcessingJobType::FinalSessionSummary:
        result = m_studyProcessor.processFinalSummary(job.sessionId, modelName);
        break;
    }

    if (result.ok) {
        const std::string completed = "AI processing job completed: " + job.typeName();
        m_storage.addModelEvent("processing_job_completed", modelName, completed + " session=" + job.sessionId);
        if (job.type == ProcessingJobType::FinalSessionSummary) {
            m_storage.setSessionSummaryStatus(job.sessionId, "complete");
        }
        emitStatus(completed);
        return;
    }

    const std::string failed = "AI processing job failed: " + job.typeName() + " - " + result.message;
    m_storage.addModelEvent("processing_job_failed", modelName, failed + " session=" + job.sessionId);
    if (job.type == ProcessingJobType::FinalSessionSummary) {
        m_storage.setSessionSummaryStatus(job.sessionId, "failed");
    }
    emitStatus(failed);
}

std::string ProcessingQueue::currentModelName()
{
    return m_storage.getSetting("ai.current_model").value_or(ai::kDefaultGemmaModel);
}

void ProcessingQueue::emitStatus(const std::string &message)
{
    StatusCallback callback;
    {
        std::lock_guard lock(m_mutex);
        callback = m_statusCallback;
    }

    if (callback) {
        callback(message);
    }
}

} // namespace local_jarvis::processing
