#include "audio/AudioLevelMeter.h"
#include "audio/DummyAudioCapture.h"
#include "privacy/PrivacyManager.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    using local_jarvis::audio::AudioLevelMeter;
    using local_jarvis::audio::DummyAudioCapture;
    using local_jarvis::privacy::PrivacyManager;

    AudioLevelMeter meter;
    const std::array<float, 32> silence {};
    meter.processSamples(silence);
    if (!expect(meter.level() <= 0.001, "Silence should produce a near-zero level.")) {
        return EXIT_FAILURE;
    }
    if (!expect(AudioLevelMeter::calculateRms(silence) <= 0.001, "Silence RMS should be near zero.")) {
        return EXIT_FAILURE;
    }
    if (!expect(AudioLevelMeter::calculatePeak(silence) == 0.0, "Silence peak should be zero.")) {
        return EXIT_FAILURE;
    }
    if (!expect(AudioLevelMeter::amplitudeToDbfs(0.0) <= -119.0, "Zero amplitude should map to a very low dBFS value.")) {
        return EXIT_FAILURE;
    }

    const std::array<float, 8> loud {
        1.0F, -1.0F, 0.9F, -0.9F,
        0.8F, -0.8F, 0.7F, -0.7F
    };
    const double loudRms = AudioLevelMeter::calculateRms(loud);
    if (!expect(loudRms > 0.8 && loudRms <= 1.0, "Float32 RMS should reflect loud normalized samples.")) {
        return EXIT_FAILURE;
    }
    if (!expect(AudioLevelMeter::calculatePeak(loud) == 1.0, "Peak helper should report normalized peak amplitude.")) {
        return EXIT_FAILURE;
    }
    if (!expect(AudioLevelMeter::nonZeroSampleRatio(loud) == 1.0, "Non-zero ratio should count audible samples.")) {
        return EXIT_FAILURE;
    }
    if (!expect(AudioLevelMeter::amplitudeToDbfs(1.0) > -0.1, "Full-scale amplitude should be near 0 dBFS.")) {
        return EXIT_FAILURE;
    }
    meter.processSamples(loud);
    const double loudLevel = meter.level();
    if (!expect(loudLevel > 0.35, "Loud samples should produce a higher level.")) {
        return EXIT_FAILURE;
    }
    if (!expect(meter.lastRms() == loudRms, "AudioLevelMeter should retain the last raw RMS value.")) {
        return EXIT_FAILURE;
    }

    const std::array<float, 4> clipped { 2.0F, -2.0F, 1.5F, -1.5F };
    if (!expect(AudioLevelMeter::calculateRms(clipped) == 1.0, "High-amplitude float samples should clip to normalized RMS 1.0.")) {
        return EXIT_FAILURE;
    }

    const std::array<std::int16_t, 4> int16Samples { 32767, -32768, 0, 16384 };
    meter.reset();
    meter.processInt16Samples(int16Samples);
    if (!expect(meter.lastRms() > 0.65 && meter.level() > 0.35, "Int16 samples should convert to normalized RMS.")) {
        return EXIT_FAILURE;
    }

    const std::array<float, 6> stereo {
        1.0F, -1.0F,
        0.5F, 0.5F,
        0.0F, 1.0F
    };
    const auto mono = AudioLevelMeter::mixInterleavedToMono(stereo, 2);
    if (!expect(mono.size() == 3, "Stereo input should mix to one mono sample per frame.")) {
        return EXIT_FAILURE;
    }
    if (!expect(std::abs(mono[0]) <= 0.001F && mono[1] > 0.49F && mono[2] > 0.49F,
            "Stereo-to-mono mix should average channels safely.")) {
        return EXIT_FAILURE;
    }

    meter.processSamples(silence);
    if (!expect(meter.level() > 0.0 && meter.level() < loudLevel, "Smoothing should decay safely after silence.")) {
        return EXIT_FAILURE;
    }

    meter.processSamples(std::span<const float> {});
    if (!expect(meter.level() >= 0.0 && meter.level() <= loudLevel, "Empty buffers should not crash or raise the level.")) {
        return EXIT_FAILURE;
    }

    meter.reset();
    if (!expect(meter.level() == 0.0, "Reset should clear the level.")) {
        return EXIT_FAILURE;
    }

    PrivacyManager privacy;
    DummyAudioCapture dummyCapture(privacy);
    const auto devices = dummyCapture.listInputDevices();
    if (!expect(!devices.empty(), "Dummy capture should expose a test input device.")) {
        return EXIT_FAILURE;
    }
    if (!expect(!dummyCapture.startMicrophoneCapture(), "Dummy capture must not start before explicit microphone permission.")) {
        return EXIT_FAILURE;
    }

    privacy.setMicrophoneEnabled(true);
    if (!expect(dummyCapture.startMicrophoneCapture(), "Dummy capture should start after explicit microphone permission.")) {
        return EXIT_FAILURE;
    }
    if (!expect(dummyCapture.isMicrophoneActive(), "Dummy microphone should report active after start.")) {
        return EXIT_FAILURE;
    }
    if (!expect(dummyCapture.currentInputLevel() > 0.0, "Dummy capture should expose a visible test input level.")) {
        return EXIT_FAILURE;
    }
    const auto diagnostics = dummyCapture.diagnostics();
    if (!expect(diagnostics.captureActive, "Dummy diagnostics should report active capture.")) {
        return EXIT_FAILURE;
    }
    if (!expect(diagnostics.sampleRate == 16000 && diagnostics.channelCount == 1, "Dummy diagnostics should report stable test format.")) {
        return EXIT_FAILURE;
    }
    if (!expect(diagnostics.lastBufferPeak > 0.0 && diagnostics.lastBufferDbfs > -30.0 && diagnostics.lastBufferNonZeroRatio > 0.9,
            "Dummy diagnostics should expose peak, dBFS, and non-zero sample ratio.")) {
        return EXIT_FAILURE;
    }
    dummyCapture.stopMicrophoneCapture();
    if (!expect(!dummyCapture.isMicrophoneActive(), "Dummy microphone should stop cleanly.")) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
