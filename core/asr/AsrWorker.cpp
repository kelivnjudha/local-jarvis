#include "AsrWorker.h"

#include "WhisperAudioConversion.h"
#include "audio/AudioLevelMeter.h"
#include "audio/AudioSpeechDetector.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>

namespace local_jarvis::asr {
namespace {

std::string trimAscii(std::string value)
{
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string lowercaseAscii(std::string value)
{
    for (char &character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

bool isBlankAsrText(const std::string &text)
{
    const std::string trimmed = trimAscii(text);
    if (trimmed.empty()) {
        return true;
    }
    return lowercaseAscii(trimmed) == "[blank_audio]";
}

AsrInputChunk whisperChunkFromSamples(const AsrInputChunk &chunk, std::vector<float> samples)
{
    AsrInputChunk adjusted = chunk;
    adjusted.sampleRate = kWhisperSampleRate;
    adjusted.channels = 1;
    adjusted.samples = std::move(samples);
    return adjusted;
}

AsrSourceStats &sourceStatsFor(AsrWorkerStats &stats, AsrAudioSource source)
{
    return source == AsrAudioSource::SystemAudio ? stats.systemAudio : stats.microphone;
}

} // namespace

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
            return false;
        }
        if (!modelPath.empty()) {
            m_config.modelPath = modelPath;
        }
    }

    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = false;
        m_running = true;
        m_lastError.clear();
        m_status = AsrStatus::Loading;
    }

    m_worker = std::thread([this]() {
        workerLoop();
    });
    publishStatus(AsrStatus::Loading, "ASR worker loading.");
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

void AsrWorker::setEngine(std::unique_ptr<AsrEngine> engine)
{
    stop();
    std::lock_guard lock(m_mutex);
    m_engine = std::move(engine);
    m_chunksQueued = 0;
    m_chunksProcessed = 0;
    m_lastDiagnostics = {};
    m_lastError.clear();
    m_status = m_engine ? AsrStatus::Disabled : AsrStatus::Error;
}

void AsrWorker::setConfig(const AsrEngineConfig &config)
{
    std::lock_guard lock(m_mutex);
    m_config = config;
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
        ++sourceStatsFor(m_lastDiagnostics, chunk.audioSource).chunksQueued;
        m_status = AsrStatus::Processing;
    }
    m_condition.notify_one();
    publishStatus(AsrStatus::Processing, "ASR chunk queued.");
}

void AsrWorker::setListening(bool listening)
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_running
            || m_status == AsrStatus::Loading
            || m_status == AsrStatus::Processing
            || m_status == AsrStatus::Error
            || !m_queue.empty()) {
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
    AsrWorkerStats stats = m_lastDiagnostics;
    stats.status = m_status;
    stats.chunksQueued = m_chunksQueued;
    stats.chunksProcessed = m_chunksProcessed;
    stats.pendingChunks = m_queue.size();
    stats.lastError = m_lastError;
    stats.microphone.pendingChunks = 0;
    stats.systemAudio.pendingChunks = 0;
    for (const auto &chunk : m_queue) {
        ++sourceStatsFor(stats, chunk.audioSource).pendingChunks;
    }
    return stats;
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
    AsrEngineConfig config;
    {
        std::lock_guard lock(m_mutex);
        config = m_config;
    }

    m_engine->configure(config);
    const bool whisperBackend = m_engine->engineName() == "Whisper";
    const audio::AudioSpeechDetector speechDetector(config.speechDetection);
    if (!m_engine->isInitialized() && !m_engine->initialize(config.modelPath)) {
        const std::string error = m_engine->lastError().empty()
            ? "ASR engine initialization failed."
            : m_engine->lastError();
        {
            std::lock_guard lock(m_mutex);
            m_queue.clear();
            m_running = false;
            m_lastError = error;
            m_status = AsrStatus::Error;
        }
        publishStatus(AsrStatus::Error, error);
        return;
    }

    publishStatus(AsrStatus::Ready, "ASR worker ready.");

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

        auto whisperSamples = prepareWhisperSamples(chunk);
        const std::int64_t chunkDurationMs = std::max<std::int64_t>(0, chunk.endMs - chunk.startMs);
        const double chunkRms = normalizedRms(whisperSamples);
        const double chunkPeak = audio::AudioLevelMeter::calculatePeak(whisperSamples);
        const double chunkDbfs = audio::AudioLevelMeter::amplitudeToDbfs(chunkRms);
        const double chunkNonZeroRatio = audio::AudioLevelMeter::nonZeroSampleRatio(whisperSamples);
        const bool chunkSilent = whisperSamples.empty() || isProbablySilent(whisperSamples);
        const auto speechState = speechDetector.classify(audio::AudioSpeechFeatures {
            .rms = chunkRms,
            .peak = chunkPeak,
            .dbfs = chunkDbfs,
            .nonZeroPercentage = chunkNonZeroRatio,
            .chunkDurationMs = static_cast<int>(chunkDurationMs)
        });
        {
            std::lock_guard lock(m_mutex);
            m_lastDiagnostics.lastAudioSource = chunk.audioSource;
            m_lastDiagnostics.lastChunkId = chunk.chunkId;
            m_lastDiagnostics.lastChunkDurationMs = chunkDurationMs;
            m_lastDiagnostics.lastChunkSampleRate = chunk.sampleRate;
            m_lastDiagnostics.lastChunkChannels = chunk.channels;
            m_lastDiagnostics.lastChunkInputSamples = chunk.samples.size();
            m_lastDiagnostics.lastWhisperSampleCount = whisperSamples.size();
            m_lastDiagnostics.lastChunkRms = chunkRms;
            m_lastDiagnostics.lastChunkPeak = chunkPeak;
            m_lastDiagnostics.lastChunkDbfs = chunkDbfs;
            m_lastDiagnostics.lastChunkNonZeroRatio = chunkNonZeroRatio;
            m_lastDiagnostics.lastChunkTreatedAsSilent = chunkSilent;
            m_lastDiagnostics.lastSpeechDetectionState = audio::displayName(speechState);
            m_lastDiagnostics.preprocessingEnabled = whisperBackend && config.preprocessing.enabled;
            m_lastDiagnostics.lastPreprocessingGainDb = 0.0;
            m_lastDiagnostics.lastPreprocessingLimiterEngaged = false;
            auto &sourceStats = sourceStatsFor(m_lastDiagnostics, chunk.audioSource);
            sourceStats.lastChunkId = chunk.chunkId;
            sourceStats.lastChunkDurationMs = chunkDurationMs;
            sourceStats.lastChunkSampleRate = chunk.sampleRate;
            sourceStats.lastChunkChannels = chunk.channels;
            sourceStats.lastChunkInputSamples = chunk.samples.size();
            sourceStats.lastWhisperSampleCount = whisperSamples.size();
            sourceStats.lastChunkRms = chunkRms;
            sourceStats.lastChunkPeak = chunkPeak;
            sourceStats.lastChunkDbfs = chunkDbfs;
            sourceStats.lastChunkNonZeroRatio = chunkNonZeroRatio;
            sourceStats.lastChunkTreatedAsSilent = chunkSilent;
            sourceStats.lastSpeechDetectionState = audio::displayName(speechState);
        }

        if (whisperBackend && speechState == audio::SpeechDetectionState::Silence) {
            {
                std::lock_guard lock(m_mutex);
                ++m_chunksProcessed;
                ++m_lastDiagnostics.chunksSkippedSilence;
                m_status = m_queue.empty() ? AsrStatus::Listening : AsrStatus::Processing;
                m_lastDiagnostics.lastTranscriptText = "Skipped silent chunk.";
                auto &sourceStats = sourceStatsFor(m_lastDiagnostics, chunk.audioSource);
                ++sourceStats.chunksProcessed;
                ++sourceStats.chunksSkippedSilence;
                sourceStats.lastTranscriptText = m_lastDiagnostics.lastTranscriptText;
            }
            publishStatus(stats().status, "Waiting for speech...");
            continue;
        }

        if (whisperBackend
            && speechState == audio::SpeechDetectionState::TooQuiet
            && config.skipTooQuietChunks
            && !config.debugProcessTooQuiet) {
            {
                std::lock_guard lock(m_mutex);
                ++m_chunksProcessed;
                ++m_lastDiagnostics.chunksSkippedTooQuiet;
                m_status = m_queue.empty() ? AsrStatus::Listening : AsrStatus::Processing;
                m_lastDiagnostics.lastTranscriptText = "Skipped too-quiet chunk.";
                auto &sourceStats = sourceStatsFor(m_lastDiagnostics, chunk.audioSource);
                ++sourceStats.chunksProcessed;
                ++sourceStats.chunksSkippedTooQuiet;
                sourceStats.lastTranscriptText = m_lastDiagnostics.lastTranscriptText;
            }
            publishStatus(stats().status, "Input too quiet for transcription");
            continue;
        }

        AsrInputChunk engineChunk = chunk;
        if (whisperBackend && config.preprocessing.enabled) {
            const auto preprocessingResult = preprocessWhisperSamples(whisperSamples, config.preprocessing);
            whisperSamples = preprocessingResult.samples;
            engineChunk = whisperChunkFromSamples(chunk, whisperSamples);
            {
                std::lock_guard lock(m_mutex);
                m_lastDiagnostics.lastWhisperSampleCount = whisperSamples.size();
                m_lastDiagnostics.lastPreprocessingGainDb = preprocessingResult.appliedGainDb;
                m_lastDiagnostics.lastPreprocessingLimiterEngaged = preprocessingResult.limiterEngaged;
            }
            if (preprocessingResult.appliedGainDb > 0.0) {
                publishStatus(AsrStatus::Processing, "ASR preprocessing applied.");
            }
        }

        publishStatus(AsrStatus::Processing, whisperBackend ? "Processing speech..." : "ASR processing chunk.");
        const auto result = m_engine->transcribeChunk(engineChunk);
        AsrTranscriptSegment resultSegment = result.segment;
        resultSegment.audioSource = chunk.audioSource;
        if (resultSegment.speaker.empty()
            || (chunk.audioSource == AsrAudioSource::SystemAudio && resultSegment.speaker == "Microphone")) {
            resultSegment.speaker = captionSpeakerForSource(chunk.audioSource);
        }

        SegmentCallback segmentCallback;
        if (result.ok) {
            const bool blankOutput = isBlankAsrText(result.text);
            {
                std::lock_guard lock(m_mutex);
                ++m_chunksProcessed;
                auto &sourceStats = sourceStatsFor(m_lastDiagnostics, chunk.audioSource);
                ++sourceStats.chunksProcessed;
                m_lastError.clear();
                m_status = m_queue.empty() ? AsrStatus::Listening : AsrStatus::Processing;
                m_lastDiagnostics.lastTranscriptText = result.text.empty() ? result.message : result.text;
                sourceStats.lastTranscriptText = m_lastDiagnostics.lastTranscriptText;
                if (blankOutput) {
                    ++m_lastDiagnostics.blankOutputs;
                    m_lastDiagnostics.lastTranscriptText = result.text.empty() ? "Blank ASR output suppressed." : "Blank ASR output suppressed: " + result.text;
                    ++sourceStats.blankOutputs;
                    sourceStats.lastTranscriptText = m_lastDiagnostics.lastTranscriptText;
                } else {
                    segmentCallback = m_segmentCallback;
                }
            }
            if (blankOutput) {
                publishStatus(stats().status, "ASR blank output suppressed.");
                continue;
            }
            if (segmentCallback) {
                segmentCallback(resultSegment);
            }
            publishStatus(stats().status, "ASR chunk processed.");
        } else {
            const std::string error = result.message.empty() ? "ASR chunk processing failed." : result.message;
            {
                std::lock_guard lock(m_mutex);
                ++m_chunksProcessed;
                m_lastError = error;
                m_status = AsrStatus::Error;
                m_lastDiagnostics.lastTranscriptText = result.text.empty() ? error : result.text;
                auto &sourceStats = sourceStatsFor(m_lastDiagnostics, chunk.audioSource);
                ++sourceStats.chunksProcessed;
                sourceStats.lastTranscriptText = m_lastDiagnostics.lastTranscriptText;
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
        m_lastDiagnostics.lastStatusMessage = message;
        sourceStatsFor(m_lastDiagnostics, m_lastDiagnostics.lastAudioSource).lastStatusMessage = message;
    }
    if (callback) {
        callback(status, message);
    }
}

} // namespace local_jarvis::asr
