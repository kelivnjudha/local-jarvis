#include "AudioSpeechDetector.h"

#include <algorithm>
#include <cmath>

namespace local_jarvis::audio {

AudioSpeechDetector::AudioSpeechDetector(AudioSpeechDetectorConfig config)
    : m_config(config)
{
    m_config.silenceDbfsThreshold = std::clamp(m_config.silenceDbfsThreshold, -120.0, 0.0);
    m_config.tooQuietDbfsThreshold = std::clamp(m_config.tooQuietDbfsThreshold, m_config.silenceDbfsThreshold, 0.0);
    m_config.speechLikelyDbfsThreshold = std::clamp(m_config.speechLikelyDbfsThreshold, m_config.tooQuietDbfsThreshold, 0.0);
    m_config.clippingPeakThreshold = std::clamp(m_config.clippingPeakThreshold, 0.20, 1.0);
    m_config.minNonZeroPercentage = std::clamp(m_config.minNonZeroPercentage, 0.0, 1.0);
    m_config.minChunkDurationMs = std::max(0, m_config.minChunkDurationMs);
}

SpeechDetectionState AudioSpeechDetector::classify(const AudioSpeechFeatures &features) const
{
    const double rms = std::clamp(features.rms, 0.0, 1.0);
    const double peak = std::clamp(features.peak, 0.0, 1.0);
    const double nonZero = std::clamp(features.nonZeroPercentage, 0.0, 1.0);
    const double dbfs = std::isfinite(features.dbfs) ? features.dbfs : -120.0;

    if (features.chunkDurationMs < m_config.minChunkDurationMs
        || nonZero < m_config.minNonZeroPercentage
        || dbfs <= m_config.silenceDbfsThreshold
        || (rms <= 0.000001 && peak <= 0.000001)) {
        return SpeechDetectionState::Silence;
    }

    if (peak >= m_config.clippingPeakThreshold) {
        return SpeechDetectionState::ClippingRisk;
    }

    if (dbfs < m_config.tooQuietDbfsThreshold) {
        return SpeechDetectionState::TooQuiet;
    }

    if (dbfs >= m_config.speechLikelyDbfsThreshold
        || (features.smoothedLevel >= 0.0 && features.smoothedLevel >= rms && features.smoothedLevel >= 0.05)) {
        return SpeechDetectionState::SpeechLikely;
    }

    return SpeechDetectionState::MaybeSpeech;
}

const AudioSpeechDetectorConfig &AudioSpeechDetector::config() const
{
    return m_config;
}

std::string toString(SpeechDetectionState state)
{
    switch (state) {
    case SpeechDetectionState::Silence:
        return "silence";
    case SpeechDetectionState::TooQuiet:
        return "too_quiet";
    case SpeechDetectionState::MaybeSpeech:
        return "maybe_speech";
    case SpeechDetectionState::SpeechLikely:
        return "speech_likely";
    case SpeechDetectionState::ClippingRisk:
        return "clipping_risk";
    }
    return "silence";
}

std::string displayName(SpeechDetectionState state)
{
    switch (state) {
    case SpeechDetectionState::Silence:
        return "Silence";
    case SpeechDetectionState::TooQuiet:
        return "Too quiet";
    case SpeechDetectionState::MaybeSpeech:
        return "Maybe speech";
    case SpeechDetectionState::SpeechLikely:
        return "Speech likely";
    case SpeechDetectionState::ClippingRisk:
        return "Clipping risk";
    }
    return "Silence";
}

} // namespace local_jarvis::audio
