#pragma once

#include <cstddef>

namespace marble::core {

/// Alignment constants for SIMD-friendly buffers (Ch. 4.10). Use with `MemoryLayout::alignUp` for sizes.
inline constexpr std::size_t kSimd128AlignBytes = 16;
inline constexpr std::size_t kSimd256AlignBytes = 32;
inline constexpr std::size_t kSimd512AlignBytes = 64;

/// Returns a conservative alignment for buffers holding `vectorWidthBits`-wide SIMD values (128 / 256 / 512+).
[[nodiscard]] constexpr std::size_t simdBufferAlignBytes(unsigned vectorWidthBits) noexcept {
    if (vectorWidthBits <= 128) {
        return kSimd128AlignBytes;
    }
    if (vectorWidthBits <= 256) {
        return kSimd256AlignBytes;
    }
    return kSimd512AlignBytes;
}

} // namespace marble::core
