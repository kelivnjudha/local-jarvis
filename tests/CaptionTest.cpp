#include "caption/CaptionFormatter.h"
#include "caption/CaptionManager.h"
#include "caption/DummyCaptionSource.h"
#include "storage/Storage.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

local_jarvis::caption::CaptionSegment sampleSegment()
{
    return local_jarvis::caption::CaptionSegment {
        .id = "test-caption-1",
        .speaker = "Teacher",
        .originalText = "Benedict's solution tests for reducing sugars.",
        .translatedText = "Benedict's solution tests for reducing sugars.",
        .summaryText = "Benedict's solution identifies reducing sugars by colour change.",
        .detectedLanguage = "en",
        .startMs = 100,
        .endMs = 2100,
        .isFinal = true
    };
}

bool testFormattingModes()
{
    using local_jarvis::caption::CaptionFormatter;
    using local_jarvis::caption::CaptionMode;
    using local_jarvis::caption::CaptionState;

    CaptionFormatter formatter;
    CaptionState state;
    state.latestSegments.push_back(sampleSegment());

    state.captionMode = CaptionMode::OriginalOnly;
    if (!expect(formatter.format(state) == "Teacher: Benedict's solution tests for reducing sugars.", "OriginalOnly formatting failed.")) {
        return false;
    }

    state.captionMode = CaptionMode::TranslationOnly;
    if (!expect(formatter.format(state) == "EN: Benedict's solution tests for reducing sugars.", "TranslationOnly formatting failed.")) {
        return false;
    }

    state.captionMode = CaptionMode::OriginalAndTranslation;
    auto thai = sampleSegment();
    thai.originalText = "Reducing sugar original.";
    thai.translatedText = "Reducing sugar translation.";
    state.latestSegments = { thai };
    if (!expect(formatter.format(state) == "Teacher: Reducing sugar original.\nEN: Reducing sugar translation.",
            "OriginalAndTranslation formatting failed.")) {
        return false;
    }

    state.captionMode = CaptionMode::CleanSummary;
    if (!expect(formatter.format(state) == "Key point: Benedict's solution identifies reducing sugars by colour change.",
            "CleanSummary formatting failed.")) {
        return false;
    }

    state.captionMode = CaptionMode::Off;
    return expect(formatter.format(state).empty(), "Off mode should return empty text.");
}

bool testLimitsAndFallbacks()
{
    using local_jarvis::caption::CaptionFormatter;
    using local_jarvis::caption::CaptionMode;
    using local_jarvis::caption::CaptionState;

    CaptionFormatter formatter;
    CaptionState state;
    auto first = sampleSegment();
    auto second = sampleSegment();
    second.id = "test-caption-2";
    second.originalText = "Second original line.";
    second.translatedText = "Second translated line.";
    state.latestSegments = { first, second };
    state.captionMode = CaptionMode::OriginalAndTranslation;
    state.maxLines = 1;
    if (!expect(formatter.format(state).find('\n') == std::string::npos, "maxLines should limit output lines.")) {
        return false;
    }

    state.maxLines = 4;
    state.maxCharacters = 24;
    const auto limited = formatter.format(state);
    if (!expect(limited.size() <= 24, "maxCharacters should limit output length.")) {
        return false;
    }

    CaptionState fallbackState;
    fallbackState.captionMode = CaptionMode::TranslationOnly;
    fallbackState.latestSegments.push_back(local_jarvis::caption::CaptionSegment {
        .id = "missing-translation",
        .speaker = "Teacher",
        .originalText = "Fallback original.",
        .translatedText = "",
        .summaryText = "",
        .detectedLanguage = "en",
        .startMs = 0,
        .endMs = 1000,
        .isFinal = true
    });
    return expect(formatter.format(fallbackState) == "EN: Fallback original.", "Missing translation should fall back safely.");
}

bool testCaptionManager()
{
    using local_jarvis::caption::CaptionManager;
    using local_jarvis::caption::CaptionMode;
    using local_jarvis::storage::Storage;

    const auto dbPath = std::filesystem::temp_directory_path() / "local-jarvis-caption-test.sqlite";
    std::filesystem::remove(dbPath);

    Storage storage;
    if (!storage.initialize(dbPath)) {
        std::cerr << "Failed to initialize storage: " << storage.lastError() << '\n';
        return false;
    }

    CaptionManager manager(storage);
    if (!expect(manager.loadSettings(), "Caption settings should load.")) {
        return false;
    }
    manager.setCaptionMode(CaptionMode::OriginalOnly);
    manager.setCaptionsEnabled(true);
    manager.setShowSpeaker(false);
    manager.setMaxLines(3);
    manager.setMaxCharacters(120);
    manager.setSourceLanguage("auto");
    manager.setTargetLanguage("en");
    manager.addSegment(sampleSegment());
    manager.addSegment(sampleSegment());
    const auto duplicatesSuppressed = manager.duplicateSuppressedCount();
    const auto storedAfterDuplicate = manager.state().latestSegments.size();
    const auto displayBeforeClear = manager.currentDisplayText();
    manager.clearSegments();
    const auto heldText = manager.currentDisplayText();

    CaptionManager reloaded(storage);
    const bool ok = expect(storedAfterDuplicate == 1, "CaptionManager should store latest non-duplicate segment in memory.")
        && expect(displayBeforeClear == "Benedict's solution tests for reducing sugars.", "CaptionManager display text failed.")
        && expect(duplicatesSuppressed == 1, "CaptionManager should suppress duplicate caption segments.")
        && expect(heldText == displayBeforeClear, "CaptionManager should hold the last useful caption briefly after segments clear.")
        && expect(reloaded.loadSettings(), "Reloaded caption settings should load.")
        && expect(reloaded.state().captionMode == CaptionMode::OriginalOnly, "Caption mode did not persist.")
        && expect(!reloaded.state().showSpeaker, "Show speaker setting did not persist.")
        && expect(reloaded.state().maxLines == 3, "Max lines did not persist.")
        && expect(reloaded.state().maxCharacters == 120, "Max characters did not persist.");

    storage.close();
    std::filesystem::remove(dbPath);
    return ok;
}

bool testDummyCaptionSource()
{
    local_jarvis::caption::DummyCaptionSource source;
    const auto &samples = source.samples();
    bool hasThai = false;
    bool hasBurmese = false;
    bool hasVietnamese = false;
    bool hasChinese = false;
    for (const auto &sample : samples) {
        if (!expect(!sample.originalText.empty(), "Dummy caption original text should not be empty.")
            || !expect(!sample.translatedText.empty(), "Dummy caption translated text should not be empty.")
            || !expect(!sample.summaryText.empty(), "Dummy caption summary text should not be empty.")
            || !expect(!sample.detectedLanguage.empty(), "Dummy caption language should not be empty.")
            || !expect(!sample.speaker.empty(), "Dummy caption speaker should not be empty.")) {
            return false;
        }
        hasThai = hasThai || sample.detectedLanguage == "th";
        hasBurmese = hasBurmese || sample.detectedLanguage == "my";
        hasVietnamese = hasVietnamese || sample.detectedLanguage == "vi";
        hasChinese = hasChinese || sample.detectedLanguage == "zh";
    }

    const auto first = source.nextSegment();
    const auto second = source.nextSegment();
    return expect(first.id != second.id, "DummyCaptionSource should emit unique sample ids.")
        && expect(hasThai && hasBurmese && hasVietnamese && hasChinese, "DummyCaptionSource should include multilingual samples.");
}

} // namespace

int main()
{
    const bool ok = testFormattingModes()
        && testLimitsAndFallbacks()
        && testCaptionManager()
        && testDummyCaptionSource();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
