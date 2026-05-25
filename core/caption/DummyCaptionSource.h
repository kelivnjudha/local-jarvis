#pragma once

#include "CaptionTypes.h"

#include <cstddef>
#include <vector>

namespace local_jarvis::caption {

class DummyCaptionSource {
public:
    [[nodiscard]] CaptionSegment nextSegment();
    [[nodiscard]] const std::vector<CaptionSegment> &samples() const;
    void reset();

private:
    std::size_t m_index = 0;
};

} // namespace local_jarvis::caption
