#pragma once

#include <bit>
#include <cstddef>

namespace marble::core {

/// Conventional cache line size (bytes) for false-sharing and padding decisions.
/// Not queried from the CPU; override at call sites if you need a different value.
inline constexpr std::size_t kCacheLineBytes = 64;

/// True if \p x is a non-zero power of two (valid alignment for `alignUp` / `alignDown`).
constexpr bool isPowerOfTwo(std::size_t x) noexcept {
    return x != 0 && std::has_single_bit(x);
}

/// Round \p value up to a multiple of \p alignment.
/// Precondition: `isPowerOfTwo(alignment)`.
constexpr std::size_t alignUp(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

/// Round \p value down to a multiple of \p alignment.
/// Precondition: `isPowerOfTwo(alignment)`.
constexpr std::size_t alignDown(std::size_t value, std::size_t alignment) noexcept {
    return value & ~(alignment - 1);
}

} // namespace marble::core
