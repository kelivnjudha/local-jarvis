#include "DummyCaptionSource.h"

#include <string>

namespace local_jarvis::caption {
namespace {

std::string utf8(const char8_t *text)
{
    return reinterpret_cast<const char *>(text);
}

const std::vector<CaptionSegment> &sampleSegments()
{
    static const std::vector<CaptionSegment> segments {
        CaptionSegment {
            .id = "dummy-caption-en",
            .speaker = "Teacher",
            .originalText = "Benedict's solution tests for reducing sugars.",
            .translatedText = "Benedict's solution tests for reducing sugars.",
            .summaryText = "Benedict's solution identifies reducing sugars by colour change.",
            .detectedLanguage = "en",
            .startMs = 0,
            .endMs = 2800,
            .isFinal = true
        },
        CaptionSegment {
            .id = "dummy-caption-th",
            .speaker = "Teacher",
            .originalText = utf8(u8"น้ำตาลรีดิวซ์ทดสอบด้วย Benedict's solution."),
            .translatedText = "Reducing sugar is tested with Benedict's solution.",
            .summaryText = "Benedict's solution is used to test for reducing sugars.",
            .detectedLanguage = "th",
            .startMs = 2800,
            .endMs = 6200,
            .isFinal = true
        },
        CaptionSegment {
            .id = "dummy-caption-my",
            .speaker = "Student",
            .originalText = utf8(u8"ဒီနေ့ အဓိက အချက်ကို မှတ်ထားပါ။"),
            .translatedText = "Remember today's key point.",
            .summaryText = "The speaker asks the student to remember the key point.",
            .detectedLanguage = "my",
            .startMs = 6200,
            .endMs = 9100,
            .isFinal = true
        },
        CaptionSegment {
            .id = "dummy-caption-vi",
            .speaker = "Coach",
            .originalText = utf8(u8"Hãy trả lời chậm và rõ ràng."),
            .translatedText = "Answer slowly and clearly.",
            .summaryText = "Interview practice should be slow and clear.",
            .detectedLanguage = "vi",
            .startMs = 9100,
            .endMs = 11800,
            .isFinal = true
        },
        CaptionSegment {
            .id = "dummy-caption-zh",
            .speaker = "Reviewer",
            .originalText = utf8(u8"复习时先总结重点。"),
            .translatedText = "When reviewing, summarize the key points first.",
            .summaryText = "Start review by summarizing key points.",
            .detectedLanguage = "zh",
            .startMs = 11800,
            .endMs = 14600,
            .isFinal = true
        }
    };
    return segments;
}

} // namespace

CaptionSegment DummyCaptionSource::nextSegment()
{
    const auto &items = sampleSegments();
    CaptionSegment segment = items[m_index % items.size()];
    segment.id += "-" + std::to_string(m_index);
    ++m_index;
    return segment;
}

const std::vector<CaptionSegment> &DummyCaptionSource::samples() const
{
    return sampleSegments();
}

void DummyCaptionSource::reset()
{
    m_index = 0;
}

} // namespace local_jarvis::caption
