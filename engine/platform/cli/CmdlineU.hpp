#pragma once

#include <cstdint>
#include <cstdlib>

namespace marble::platform::cli {

/// Strict decimal `strtoul`: entire string consumed, value in `0..65535`.
[[nodiscard]] inline bool parseU16(char const* s, std::uint16_t& out) noexcept {
    if (s == nullptr || s[0] == '\0') {
        return false;
    }
    char* end{};
    unsigned long const v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0' || v > 65535ul) {
        return false;
    }
    out = static_cast<std::uint16_t>(v);
    return true;
}

} // namespace marble::platform::cli
