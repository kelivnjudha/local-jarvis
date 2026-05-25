#pragma once

#include "AsrTypes.h"

#include <span>
#include <vector>

namespace local_jarvis::asr {

constexpr int kWhisperSampleRate = 16000;

[[nodiscard]] std::vector<float> resampleToWhisperRate(
    std::span<const float> monoSamples,
    int inputSampleRate,
    int outputSampleRate = kWhisperSampleRate);

[[nodiscard]] std::vector<float> prepareWhisperSamples(const AsrInputChunk &chunk);
[[nodiscard]] double normalizedRms(std::span<const float> samples);
[[nodiscard]] bool isProbablySilent(std::span<const float> samples, double threshold = 0.0008);

} // namespace local_jarvis::asr
