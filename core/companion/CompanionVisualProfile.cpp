#include "CompanionVisualProfile.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <system_error>

namespace local_jarvis::companion {
namespace {

int clampChannel(int value)
{
    return std::clamp(value, 0, 255);
}

bool parseHexByte(const char *begin, const char *end, int &value)
{
    unsigned int parsed = 0;
    const auto result = std::from_chars(begin, end, parsed, 16);
    if (result.ec != std::errc() || result.ptr != end || parsed > 255) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

} // namespace

CompanionColor colorFromHex(const std::string &hex, CompanionColor fallback)
{
    const std::string value = !hex.empty() && hex.front() == '#'
        ? hex.substr(1)
        : hex;
    if (value.size() != 6) {
        return fallback;
    }

    CompanionColor color;
    if (!parseHexByte(value.data(), value.data() + 2, color.red)
        || !parseHexByte(value.data() + 2, value.data() + 4, color.green)
        || !parseHexByte(value.data() + 4, value.data() + 6, color.blue)) {
        return fallback;
    }

    return color;
}

std::string colorToHex(const CompanionColor &color)
{
    std::array<char, 8> buffer {};
    std::snprintf(
        buffer.data(),
        buffer.size(),
        "#%02X%02X%02X",
        clampChannel(color.red),
        clampChannel(color.green),
        clampChannel(color.blue));
    return std::string(buffer.data());
}

} // namespace local_jarvis::companion
