#pragma once

#include "MeetingNoteProcessor.h"
#include "ProcessingJob.h"
#include "StudyNoteProcessor.h"
#include "ai/OllamaClient.h"
#include "storage/Storage.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace local_jarvis::processing {

class ProcessingQueue {
public:
    using StatusCallback = std::function<void(const std::string &)>;

    ProcessingQueue(storage::Storage &storage, ai::OllamaClient &ollamaClient);
    ~ProcessingQueue();

    ProcessingQueue(const ProcessingQueue &) = delete;
    ProcessingQueue &operator=(const ProcessingQueue &) = delete;

    void start();
    void stop();

    void setStatusCallback(StatusCallback callback);
    void enqueueStudyChunk(const std::string &sessionId);
    void enqueueMeetingChunk(const std::string &sessionId);
    void enqueueFinalSessionSummary(const std::string &sessionId);

    [[nodiscard]] bool isRunning() const;

private:
    void enqueue(ProcessingJob job);
    void workerLoop();
    void runJob(const ProcessingJob &job);
    [[nodiscard]] std::string currentModelName();
    void emitStatus(const std::string &message);

    storage::Storage &m_storage;
    ai::OllamaClient &m_ollamaClient;
    StudyNoteProcessor m_studyProcessor;
    MeetingNoteProcessor m_meetingProcessor;

    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::queue<ProcessingJob> m_jobs;
    std::thread m_worker;
    bool m_stopRequested = false;
    bool m_runningJob = false;
    StatusCallback m_statusCallback;
};

} // namespace local_jarvis::processing
