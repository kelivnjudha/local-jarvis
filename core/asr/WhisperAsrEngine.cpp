#include "WhisperAsrEngine.h"

#include "WhisperAudioConversion.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

#if LOCAL_JARVIS_ENABLE_WHISPER && LOCAL_JARVIS_WHISPER_LINKED
#include <whisper.h>
#endif

namespace local_jarvis::asr {
namespace {

std::string trimWhitespace(std::string value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\n' || value.front() == '\r' || value.front() == '\t')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\n' || value.back() == '\r' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

std::string normalizeLanguage(std::string language)
{
    if (language.empty()) {
        return "auto";
    }
    std::transform(language.begin(), language.end(), language.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (language == "english") {
        return "en";
    }
    if (language == "thai") {
        return "th";
    }
    if (language == "burmese" || language == "myanmar") {
        return "my";
    }
    if (language == "vietnamese") {
        return "vi";
    }
    if (language == "chinese") {
        return "zh";
    }
    return language;
}

std::string joinSegmentText(whisper_context *context)
{
    std::ostringstream stream;
#if LOCAL_JARVIS_ENABLE_WHISPER && LOCAL_JARVIS_WHISPER_LINKED
    const int segmentCount = whisper_full_n_segments(context);
    for (int index = 0; index < segmentCount; ++index) {
        const char *text = whisper_full_get_segment_text(context, index);
        if (text == nullptr) {
            continue;
        }
        if (stream.tellp() > 0) {
            stream << ' ';
        }
        stream << trimWhitespace(text);
    }
#else
    (void)context;
#endif
    return trimWhitespace(stream.str());
}

double averageTokenConfidence(whisper_context *context)
{
#if LOCAL_JARVIS_ENABLE_WHISPER && LOCAL_JARVIS_WHISPER_LINKED
    double probabilitySum = 0.0;
    int tokenCount = 0;
    const int segmentCount = whisper_full_n_segments(context);
    for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
        const int tokens = whisper_full_n_tokens(context, segmentIndex);
        for (int tokenIndex = 0; tokenIndex < tokens; ++tokenIndex) {
            probabilitySum += std::clamp<double>(
                whisper_full_get_token_p(context, segmentIndex, tokenIndex),
                0.0,
                1.0);
            ++tokenCount;
        }
    }
    if (tokenCount > 0) {
        return probabilitySum / static_cast<double>(tokenCount);
    }
    if (segmentCount > 0) {
        return std::clamp<double>(1.0 - whisper_full_get_segment_no_speech_prob(context, 0), 0.0, 1.0);
    }
#else
    (void)context;
#endif
    return 0.5;
}

std::string detectedLanguage(whisper_context *context, const std::string &configuredLanguage)
{
    if (configuredLanguage != "auto") {
        return configuredLanguage;
    }
#if LOCAL_JARVIS_ENABLE_WHISPER && LOCAL_JARVIS_WHISPER_LINKED
    const int languageId = whisper_full_lang_id(context);
    const char *language = whisper_lang_str(languageId);
    if (language != nullptr && language[0] != '\0') {
        return language;
    }
#else
    (void)context;
#endif
    return "auto";
}

} // namespace

WhisperAsrEngine::~WhisperAsrEngine()
{
    shutdown();
}

void WhisperAsrEngine::configure(const AsrEngineConfig &config)
{
    std::lock_guard lock(m_mutex);
    m_config = config;
    m_config.language = normalizeLanguage(m_config.language);
    m_config.maxThreads = std::clamp(m_config.maxThreads, 1, 16);
}

bool WhisperAsrEngine::initialize(const std::string &modelPath)
{
    const std::string path = effectiveModelPath(modelPath);
    if (path.empty()) {
        setLastError("Whisper model path is empty. Select a local ggml model file first.");
        return false;
    }
    if (!std::filesystem::exists(std::filesystem::path(path))) {
        setLastError("Whisper model file does not exist: " + path);
        return false;
    }

#if LOCAL_JARVIS_ENABLE_WHISPER && LOCAL_JARVIS_WHISPER_LINKED
    shutdown();
    auto params = whisper_context_default_params();
    whisper_context *context = whisper_init_from_file_with_params(path.c_str(), params);
    if (context == nullptr) {
        setLastError("Failed to load Whisper model from: " + path);
        return false;
    }

    {
        std::lock_guard lock(m_mutex);
        m_context = context;
        m_modelPath = path;
        m_initialized = true;
        m_lastError.clear();
    }
    return true;
#else
    setLastError("Whisper backend was requested, but Local Jarvis was built without linked whisper.cpp support.");
    return false;
#endif
}

AsrResult WhisperAsrEngine::transcribeChunk(const AsrInputChunk &chunk)
{
    if (!isReady()) {
        return {
            .ok = false,
            .message = lastError().empty() ? "Whisper backend is not ready." : lastError()
        };
    }

    auto whisperSamples = prepareWhisperSamples(chunk);
    if (whisperSamples.empty() || isProbablySilent(whisperSamples)) {
        return {
            .ok = true,
            .message = "Whisper skipped an empty or silent chunk."
        };
    }

#if LOCAL_JARVIS_ENABLE_WHISPER && LOCAL_JARVIS_WHISPER_LINKED
    whisper_context *context = nullptr;
    AsrEngineConfig config;
    {
        std::lock_guard lock(m_mutex);
        context = m_context;
        config = m_config;
    }

    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_realtime = false;
    params.print_progress = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.translate = config.translateToEnglish;
    params.no_context = true;
    params.single_segment = false;
    params.n_threads = std::clamp(config.maxThreads, 1, 16);

    const std::string language = normalizeLanguage(config.language);
    params.language = language == "auto" ? nullptr : language.c_str();

    if (whisper_full(context, params, whisperSamples.data(), static_cast<int>(whisperSamples.size())) != 0) {
        setLastError("Whisper transcription failed for the current in-memory chunk.");
        return {
            .ok = false,
            .message = lastError()
        };
    }

    const std::string text = joinSegmentText(context);
    if (text.empty()) {
        return {
            .ok = true,
            .message = "Whisper returned no transcript text for this chunk."
        };
    }

    const std::string languageCode = detectedLanguage(context, language);
    const double confidence = averageTokenConfidence(context);
    setLastError("");
    return {
        .ok = true,
        .segment = AsrTranscriptSegment {
            .id = "asr-whisper-" + std::to_string(chunk.chunkId),
            .sessionId = chunk.sessionId,
            .startMs = chunk.startMs,
            .endMs = chunk.endMs,
            .speaker = captionSpeakerForSource(chunk.audioSource),
            .text = text,
            .detectedLanguage = languageCode,
            .confidence = confidence,
            .isFinal = true,
            .audioSource = chunk.audioSource
        },
        .text = text,
        .message = "Whisper transcript segment created."
    };
#else
    (void)chunk;
    setLastError("Whisper backend was requested, but Local Jarvis was built without linked whisper.cpp support.");
    return {
        .ok = false,
        .message = lastError()
    };
#endif
}

void WhisperAsrEngine::shutdown()
{
#if LOCAL_JARVIS_ENABLE_WHISPER && LOCAL_JARVIS_WHISPER_LINKED
    whisper_context *context = nullptr;
    {
        std::lock_guard lock(m_mutex);
        context = m_context;
        m_context = nullptr;
        m_initialized = false;
    }
    if (context != nullptr) {
        whisper_free(context);
    }
#else
    std::lock_guard lock(m_mutex);
    m_initialized = false;
#endif
}

std::string WhisperAsrEngine::engineName() const
{
    return "Whisper";
}

bool WhisperAsrEngine::isInitialized() const
{
    std::lock_guard lock(m_mutex);
    return m_initialized;
}

bool WhisperAsrEngine::isReady() const
{
    std::lock_guard lock(m_mutex);
    return m_initialized && m_context != nullptr;
}

std::string WhisperAsrEngine::lastError() const
{
    std::lock_guard lock(m_mutex);
    return m_lastError;
}

std::string WhisperAsrEngine::effectiveModelPath(const std::string &modelPath) const
{
    std::lock_guard lock(m_mutex);
    if (!modelPath.empty()) {
        return modelPath;
    }
    return m_config.modelPath;
}

void WhisperAsrEngine::setLastError(const std::string &error)
{
    std::lock_guard lock(m_mutex);
    m_lastError = error;
}

} // namespace local_jarvis::asr
