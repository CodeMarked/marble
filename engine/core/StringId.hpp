#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace marble::core {

struct StringId {
    std::uint64_t value{};

    [[nodiscard]] constexpr bool operator==(StringId const&) const noexcept = default;
};

[[nodiscard]] constexpr std::uint64_t fnv1a64(std::string_view s) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    for (char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    return h;
}

[[nodiscard]] constexpr StringId makeStringId(std::string_view s) noexcept {
    return StringId{fnv1a64(s)};
}

struct StringIdHash {
    [[nodiscard]] constexpr std::size_t operator()(StringId id) const noexcept {
        return static_cast<std::size_t>(id.value);
    }
};

namespace literals {

[[nodiscard]] constexpr StringId operator""_sid(const char* str, std::size_t len) noexcept {
    return makeStringId(std::string_view{str, len});
}

} // namespace literals

} // namespace marble::core
