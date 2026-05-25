#pragma once

#include <string>

namespace local_jarvis::ocr {

struct OcrResult {
    bool ok = false;
    std::string text;
    std::string message;
};

class OcrEngine {
public:
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] OcrResult recognizePlaceholder() const;
};

} // namespace local_jarvis::ocr
