#pragma once

#include "MarbleSampleShaderNames.hpp"
#include "core/SpirvBytecode.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace marble::game_shared {

inline constexpr std::size_t kMaxMeshSpirvBytes = marble::core::kMaxSpirvBytecodeBytes;

[[nodiscard]] inline bool loadSampleMeshSpirvFromShaderDirectory(
    std::filesystem::path const& shaderDirectory,
    std::vector<std::uint8_t>& outVertSpirv,
    std::vector<std::uint8_t>& outFragSpirv,
    std::vector<std::uint8_t>& outFragEmissiveSpirv
) {
    static_assert(kMarbleSampleShaderCount == 3, "update out-params when sample shader count changes");
    using marble::platform::filesystem::readBinaryFile;
    if (!readBinaryFile(shaderDirectory / kMarbleSampleShaderSpvBasenames[0], outVertSpirv)) {
        return false;
    }
    if (!readBinaryFile(shaderDirectory / kMarbleSampleShaderSpvBasenames[1], outFragSpirv)) {
        return false;
    }
    if (!readBinaryFile(shaderDirectory / kMarbleSampleShaderSpvBasenames[2], outFragEmissiveSpirv)) {
        return false;
    }
    return marble::core::spirvBytecodeHeaderValid(std::span<std::uint8_t const>(outVertSpirv)) &&
           marble::core::spirvBytecodeHeaderValid(std::span<std::uint8_t const>(outFragSpirv)) &&
           marble::core::spirvBytecodeHeaderValid(std::span<std::uint8_t const>(outFragEmissiveSpirv));
}

} // namespace marble::game_shared
