#include "caption/CaptionCleaner.h"
#include "caption/CaptionFormatter.h"
#include "caption/CaptionManager.h"
#include "caption/CaptionMerger.h"
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

bool testCaptionCleaner()
{
    using local_jarvis::caption::CaptionCleaner;
    using local_jarvis::caption::CaptionCleanerOptions;

    CaptionCleaner cleaner;
    CaptionCleanerOptions simpleOptions;
    simpleOptions.capitalizeFirstLetter = false;
    simpleOptions.addLightPunctuation = false;

    const auto whitespace = cleaner.cleanText("  hello\t   world  ", simpleOptions);
    if (!expect(!whitespace.rejected && whitespace.text == "hello world", "CaptionCleaner should normalize whitespace.")) {
        return false;
    }

    const auto punctuation = cleaner.cleanText("wait!!!  what??", simpleOptions);
    if (!expect(!punctuation.rejected && punctuation.text == "wait! what?",
            "CaptionCleaner should collapse repeated punctuation.")) {
        return false;
    }

    if (!expect(cleaner.cleanText("[BLANK_AUDIO]").rejected
            && cleaner.cleanText("   ").rejected,
            "CaptionCleaner should reject blank/non-content tokens.")) {
        return false;
    }

    const auto polished = cleaner.cleanText("local jarvis caption test");
    return expect(polished.text == "Local jarvis caption test.",
        "CaptionCleaner should apply light capitalization and punctuation.");
}

bool testCaptionMerger()
{
    using local_jarvis::caption::CaptionMerger;
    using local_jarvis::caption::CaptionMergerOptions;
    using local_jarvis::caption::CaptionSource;

    CaptionMerger merger;
    CaptionMergerOptions options;
    auto first = sampleSegment();
    first.id = "first";
    first.speaker = "Teacher";
    first.originalText = "Short one.";
    first.translatedText = "";
    first.summaryText = "";
    first.startMs = 0;
    first.endMs = 600;
    first.source = CaptionSource::Microphone;

    auto second = first;
    second.id = "second";
    second.originalText = "Short two.";
    second.startMs = 1000;
    second.endMs = 1400;

    if (!expect(merger.canMerge(first, second, options), "CaptionMerger should merge short same-source segments.")) {
        return false;
    }
    const auto merged = merger.merge(first, second);
    if (!expect(merged.originalText == "Short one. Short two." && merged.startMs == 0 && merged.endMs == 1400,
            "CaptionMerger should preserve predictable merged text and timing.")) {
        return false;
    }

    auto system = second;
    system.source = CaptionSource::SystemAudio;
    if (!expect(!merger.canMerge(first, system, options), "CaptionMerger should not merge mic/system by default.")) {
        return false;
    }

    auto late = second;
    late.startMs = 3000;
    late.endMs = 3500;
    if (!expect(!merger.canMerge(first, late, options), "CaptionMerger should reject gaps above the limit.")) {
        return false;
    }

    CaptionMergerOptions tightOptions = options;
    tightOptions.maxCharacters = 40;
    auto longSecond = second;
    longSecond.originalText = "This second caption is long enough to exceed the merge limit.";
    return expect(!merger.canMerge(first, longSecond, tightOptions),
        "CaptionMerger should reject merges above the max character limit.");
}

bool testSourceAwareFormatting()
{
    using local_jarvis::caption::CaptionFormatter;
    using local_jarvis::caption::CaptionMode;
    using local_jarvis::caption::CaptionSegment;
    using local_jarvis::caption::CaptionSource;
    using local_jarvis::caption::CaptionSourceDisplayMode;
    using local_jarvis::caption::CaptionState;

    CaptionFormatter formatter;
    auto mic = sampleSegment();
    mic.id = "mic-caption";
    mic.speaker = "Student";
    mic.originalText = "Shared caption text.";
    mic.translatedText = "Shared caption text.";
    mic.summaryText = "Mic summary.";
    mic.startMs = 1000;
    mic.endMs = 2000;
    mic.source = CaptionSource::Microphone;

    auto system = mic;
    system.id = "system-caption";
    system.originalText = "System caption text.";
    system.translatedText = "System caption text.";
    system.summaryText = "System summary.";
    system.startMs = 500;
    system.endMs = 1500;
    system.source = CaptionSource::SystemAudio;

    CaptionState state;
    state.captionMode = CaptionMode::OriginalOnly;
    state.maxLines = 4;
    state.latestSegments = { mic, system };

    if (!expect(formatter.format(state) == "System: System caption text.\nMic: Shared caption text.",
            "Combined source formatting should be chronological with source labels.")) {
        return false;
    }

    state.sourceDisplayMode = CaptionSourceDisplayMode::SystemOnly;
    if (!expect(formatter.format(state) == "System: System caption text.",
            "System-only caption filtering failed.")) {
        return false;
    }

    state.sourceDisplayMode = CaptionSourceDisplayMode::MicrophoneOnly;
    if (!expect(formatter.format(state) == "Mic: Shared caption text.",
            "Mic-only caption filtering failed.")) {
        return false;
    }

    state.sourceDisplayMode = CaptionSourceDisplayMode::PreferSystemAudio;
    if (!expect(formatter.format(state) == "System: System caption text.",
            "Prefer-system caption filtering failed.")) {
        return false;
    }

    state.sourceDisplayMode = CaptionSourceDisplayMode::PreferMicrophone;
    if (!expect(formatter.format(state) == "Mic: Shared caption text.",
            "Prefer-mic caption filtering failed.")) {
        return false;
    }

    state.showSourceLabels = false;
    state.showSpeaker = true;
    state.sourceDisplayMode = CaptionSourceDisplayMode::CombinedChronological;
    return expect(formatter.format(state) == "Student: System caption text.\nStudent: Shared caption text.",
        "Source labels should be configurable independently from speaker labels.");
}

bool testCrossSourceDuplicateSuppression()
{
    using local_jarvis::caption::CaptionManager;
    using local_jarvis::caption::CaptionMode;
    using local_jarvis::caption::CaptionSource;
    using local_jarvis::caption::CaptionSourceDisplayMode;

    CaptionManager manager;
    manager.setCaptionMode(CaptionMode::OriginalOnly);
    manager.setCaptionsEnabled(true);
    manager.setShowSourceLabels(true);
    manager.setSourceDisplayMode(CaptionSourceDisplayMode::CombinedChronological);

    auto mic = sampleSegment();
    mic.originalText = "The duplicate lesson sentence.";
    mic.translatedText = "The duplicate lesson sentence.";
    mic.source = CaptionSource::Microphone;
    auto system = mic;
    system.id = "system-preferred";
    system.source = CaptionSource::SystemAudio;

    const bool micAccepted = manager.addSegment(mic);
    const bool systemAccepted = manager.addSegment(system);
    if (!expect(micAccepted && systemAccepted, "System audio should replace a near-duplicate mic caption.")) {
        return false;
    }
    if (!expect(manager.state().latestSegments.size() == 1
            && manager.state().latestSegments.front().source == CaptionSource::SystemAudio,
            "Cross-source duplicate should keep the preferred system-audio segment.")) {
        return false;
    }
    if (!expect(manager.currentDisplayText() == "System: The duplicate lesson sentence.",
            "Cross-source duplicate replacement should update display text.")) {
        return false;
    }

    auto micAgain = mic;
    micAgain.id = "mic-duplicate-again";
    const bool micAgainAccepted = manager.addSegment(micAgain);
    return expect(!micAgainAccepted, "Mic duplicate should be suppressed after preferred system audio.")
        && expect(manager.crossSourceDuplicateSuppressedCount() == 2,
            "Cross-source duplicate suppression counter should include replacement and later suppression.");
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
    manager.setShowSourceLabels(false);
    manager.addSegment(sampleSegment());
    manager.addSegment(sampleSegment());
    const auto firstDisplayRefresh = manager.currentDisplayText();
    const auto secondDisplayRefresh = manager.currentDisplayText();
    const auto duplicatesSuppressed = manager.duplicateSuppressedCount();
    const auto storedAfterDuplicate = manager.state().latestSegments.size();
    const auto displayBeforeClear = manager.currentDisplayText();
    manager.setSourceDisplayMode(local_jarvis::caption::CaptionSourceDisplayMode::SystemOnly);
    manager.clearSegments();
    const auto heldText = manager.currentDisplayText();

    CaptionManager reloaded(storage);
    const bool ok = expect(storedAfterDuplicate == 1, "CaptionManager should store latest non-duplicate segment in memory.")
        && expect(displayBeforeClear == "Benedict's solution tests for reducing sugars.", "CaptionManager display text failed.")
        && expect(duplicatesSuppressed == 1, "CaptionManager should suppress duplicate caption segments.")
        && expect(firstDisplayRefresh == secondDisplayRefresh, "Repeated caption display refresh should be stable.")
        && expect(heldText == displayBeforeClear, "CaptionManager should hold the last useful caption briefly after segments clear.")
        && expect(reloaded.loadSettings(), "Reloaded caption settings should load.")
        && expect(reloaded.state().captionMode == CaptionMode::OriginalOnly, "Caption mode did not persist.")
        && expect(!reloaded.state().showSpeaker, "Show speaker setting did not persist.")
        && expect(!reloaded.state().showSourceLabels, "Show source labels setting did not persist.")
        && expect(reloaded.state().sourceDisplayMode == local_jarvis::caption::CaptionSourceDisplayMode::SystemOnly,
            "Caption source display mode did not persist.")
        && expect(manager.qualityStats().displayRefreshesSkipped > 0, "CaptionManager should count no-op display refreshes.")
        && expect(reloaded.state().maxLines == 3, "Max lines did not persist.")
        && expect(reloaded.state().maxCharacters == 120, "Max characters did not persist.");

    storage.close();
    std::filesystem::remove(dbPath);
    return ok;
}

bool testCaptionManagerQuality()
{
    using local_jarvis::caption::CaptionManager;
    using local_jarvis::caption::CaptionMode;
    using local_jarvis::caption::CaptionSource;

    CaptionManager manager;
    manager.setCaptionMode(CaptionMode::OriginalOnly);
    manager.setCaptionsEnabled(true);
    manager.setShowSpeaker(false);
    manager.setShowSourceLabels(false);
    manager.setMergeShortSegments(true);
    manager.setMergeMaxGapMs(1200);
    manager.setMergeMaxCharacters(220);

    auto first = sampleSegment();
    first.id = "quality-1";
    first.speaker = "Teacher";
    first.originalText = "first short";
    first.translatedText = "";
    first.summaryText = "";
    first.startMs = 0;
    first.endMs = 500;
    first.source = CaptionSource::Microphone;

    auto second = first;
    second.id = "quality-2";
    second.originalText = "second short";
    second.startMs = 900;
    second.endMs = 1300;

    if (!expect(manager.addSegment(first) && manager.addSegment(second),
            "CaptionManager should accept clean short captions.")) {
        return false;
    }
    if (!expect(manager.state().latestSegments.size() == 1
            && manager.currentDisplayText() == "First short. Second short.",
            "CaptionManager should clean and merge short consecutive captions.")) {
        return false;
    }

    auto blank = first;
    blank.id = "quality-blank";
    blank.originalText = "[BLANK_AUDIO]";
    blank.translatedText = "";
    blank.summaryText = "";
    const bool blankAccepted = manager.addSegment(blank);
    if (!expect(!blankAccepted, "CaptionManager should reject blank ASR artifacts.")) {
        return false;
    }

    const auto stats = manager.qualityStats();
    if (!expect(stats.captionsAccepted == 2
            && stats.captionsMerged == 1
            && stats.captionsRejected == 1,
            "CaptionManager quality counters should track accepted, merged, and rejected captions.")) {
        return false;
    }

    return expect(manager.cleanTranscriptText("[BLANK_AUDIO]").empty(),
        "Transcript storage cleaner should reject blank content.");
}

bool testCleanSummaryFallback()
{
    using local_jarvis::caption::CaptionManager;
    using local_jarvis::caption::CaptionMode;

    CaptionManager manager;
    manager.setCaptionMode(CaptionMode::CleanSummary);
    manager.setCaptionsEnabled(true);
    manager.setShowSpeaker(false);
    manager.setShowSourceLabels(false);
    auto fallback = sampleSegment();
    fallback.originalText = "clean summary fallback";
    fallback.translatedText = "";
    fallback.summaryText = "";
    manager.addSegment(fallback);
    return expect(manager.currentDisplayText() == "Key point: Clean summary fallback.",
        "CleanSummary mode should fall back to cleaned caption text when summaryText is missing.");
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
        && testCaptionCleaner()
        && testCaptionMerger()
        && testSourceAwareFormatting()
        && testCrossSourceDuplicateSuppression()
        && testCaptionManager()
        && testCaptionManagerQuality()
        && testCleanSummaryFallback()
        && testDummyCaptionSource();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
