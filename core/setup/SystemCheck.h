#pragma once

#include <cstdint>

namespace local_jarvis::setup {

class SystemCheck {
public:
    [[nodiscard]] std::uint64_t totalRamBytes() const;
    [[nodiscard]] double totalRamGb() const;
};

} // namespace local_jarvis::setup
