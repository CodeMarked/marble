#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace marble::core {

inline constexpr std::size_t kMaxSpirvBytecodeBytes = static_cast<std::size_t>(16) * 1024 * 1024;

namespace spirv_bytecode_detail {

[[nodiscard]] constexpr std::uint32_t readLeU32(std::span<std::uint8_t const> bytes, std::size_t wordIndex) noexcept {
    std::size_t const o = wordIndex * 4;
    if (o + 4 > bytes.size()) {
        return 0;
    }
    return static_cast<std::uint32_t>(bytes[o]) | (static_cast<std::uint32_t>(bytes[o + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[o + 2]) << 16u) | (static_cast<std::uint32_t>(bytes[o + 3]) << 24u);
}

} // namespace spirv_bytecode_detail

/// SPIR-V module layout: 5-word header (20 bytes), magic 0x07230203, reserved word (index 4) must be 0.
[[nodiscard]] inline bool spirvBytecodeHeaderValid(std::span<std::uint8_t const> bytes) noexcept {
    using spirv_bytecode_detail::readLeU32;
    if (bytes.empty() || bytes.size() % 4 != 0 || bytes.size() < 20 || bytes.size() > kMaxSpirvBytecodeBytes) {
        return false;
    }
    if (readLeU32(bytes, 0) != 0x07230203u) {
        return false;
    }
    return readLeU32(bytes, 4) == 0u;
}

} // namespace marble::core
