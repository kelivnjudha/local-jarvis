#include "AsrWorker.h"

#include <utility>

namespace local_jarvis::asr {

AsrWorker::AsrWorker(std::unique_ptr<AsrEngine> engine)
    : m_engine(std::move(engine))
{
}

AsrWorker::~AsrWorker()
{
    stop();
}

bool AsrWorker::start(const std::string &modelPath)
{
    {
        std::lock_guard lock(m_mutex);
        if (m_running) {
            return true;
        }
        if (!m_engine) {
            m_status = AsrStatus::Error;
            m_lastError = "No ASR engine is available.";
        }
    }

    if (!m_engine) {
        publishStatus(AsrStatus::Error, "No ASR engine is available.");
        return false;
    }

    if (!m_engine->isInitialized() && !m_engine->initialize(modelPath)) {
        publishStatus(AsrStatus::Error, "ASR engine initialization failed.");
        return false;
    }

    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = false;
        m_running = true;
        m_lastError.clear();
        m_status = AsrStatus::Ready;
    }

    m_worker = std::thread([this]() {
        workerLoop();
    });
    publishStatus(AsrStatus::Ready, "ASR worker ready.");
    return true;
}

void AsrWorker::stop()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = true;
    }
    m_condition.notify_all();

    if (m_worker.joinable()) {
        m_worker.join();
    }

    if (m_engine) {
        m_engine->shutdown();
    }

    {
        std::lock_guard lock(m_mutex);
        m_queue.clear();
        m_running = false;
        m_stopRequested = false;
        m_status = AsrStatus::Disabled;
    }
    publishStatus(AsrStatus::Disabled, "ASR worker stopped.");
}

void AsrWorker::enqueueChunk(const AsrInputChunk &chunk)
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_running) {
            return;
        }
        m_queue.push_back(chunk);
        ++m_chunksQueued;
        m_status = AsrStatus::Processing;
    }
    m_condition.notify_one();
    publishStatus(AsrStatus::Processing, "ASR chunk queued.");
}

void AsrWorker::setListening(bool listening)
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_running || m_status == AsrStatus::Processing || m_status == AsrStatus::Error || !m_queue.empty()) {
            return;
        }
        m_status = listening ? AsrStatus::Listening : AsrStatus::Ready;
    }
    publishStatus(listening ? AsrStatus::Listening : AsrStatus::Ready, listening ? "ASR listening." : "ASR ready.");
}

void AsrWorker::setSegmentCallback(SegmentCallback callback)
{
    std::lock_guard lock(m_mutex);
    m_segmentCallback = std::move(callback);
}

void AsrWorker::setStatusCallback(StatusCallback callback)
{
    std::lock_guard lock(m_mutex);
    m_statusCallback = std::move(callback);
}

AsrWorkerStats AsrWorker::stats() const
{
    std::lock_guard lock(m_mutex);
    return AsrWorkerStats {
        .status = m_status,
        .chunksQueued = m_chunksQueued,
        .chunksProcessed = m_chunksProcessed,
        .pendingChunks = m_queue.size(),
        .lastError = m_lastError
    };
}

std::string AsrWorker::engineName() const
{
    return m_engine ? m_engine->engineName() : "Unavailable";
}

bool AsrWorker::isRunning() const
{
    std::lock_guard lock(m_mutex);
    return m_running;
}

void AsrWorker::workerLoop()
{
    while (true) {
        AsrInputChunk chunk;
        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this]() {
                return m_stopRequested || !m_queue.empty();
            });
            if (m_stopRequested) {
                return;
            }
            chunk = std::move(m_queue.front());
            m_queue.pop_front();
            m_status = AsrStatus::Processing;
        }

        publishStatus(AsrStatus::Processing, "ASR processing chunk.");
        const auto result = m_engine->transcribeChunk(chunk);

        SegmentCallback segmentCallback;
        if (result.ok) {
            {
                std::lock_guard lock(m_mutex);
                ++m_chunksProcessed;
                m_lastError.clear();
                m_status = m_queue.empty() ? AsrStatus::Listening : AsrStatus::Processing;
                segmentCallback = m_segmentCallback;
            }
            if (segmentCallback) {
                segmentCallback(result.segment);
            }
            publishStatus(stats().status, "ASR chunk processed.");
        } else {
            const std::string error = result.message.empty() ? "ASR chunk processing failed." : result.message;
            {
                std::lock_guard lock(m_mutex);
                ++m_chunksProcessed;
                m_lastError = error;
                m_status = AsrStatus::Error;
            }
            publishStatus(AsrStatus::Error, error);
        }
    }
}

void AsrWorker::publishStatus(AsrStatus status, const std::string &message)
{
    StatusCallback callback;
    {
        std::lock_guard lock(m_mutex);
        if (m_status != AsrStatus::Error || status == AsrStatus::Error || status == AsrStatus::Disabled) {
            m_status = status;
        }
        if (status == AsrStatus::Error) {
            m_lastError = message;
        }
        callback = m_statusCallback;
    }
    if (callback) {
        callback(status, message);
    }
}

} // namespace local_jarvis::asr
