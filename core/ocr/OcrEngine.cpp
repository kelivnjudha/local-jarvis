#include "OcrEngine.h"

namespace local_jarvis::ocr {

bool OcrEngine::isAvailable() const
{
    return false;
}

OcrResult OcrEngine::recognizePlaceholder() const
{
    return {
        .ok = false,
        .text = {},
        .message = "Local OCR is not wired yet."
    };
}

} // namespace local_jarvis::ocr
