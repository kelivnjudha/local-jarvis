#include "audio/AudioLevelMeter.h"
#include "audio/DummyAudioCapture.h"
#include "privacy/PrivacyManager.h"

#include <array>
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

    const std::array<float, 8> loud {
        1.0F, -1.0F, 0.9F, -0.9F,
        0.8F, -0.8F, 0.7F, -0.7F
    };
    meter.processSamples(loud);
    const double loudLevel = meter.level();
    if (!expect(loudLevel > 0.35, "Loud samples should produce a higher level.")) {
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
    dummyCapture.stopMicrophoneCapture();
    if (!expect(!dummyCapture.isMicrophoneActive(), "Dummy microphone should stop cleanly.")) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
