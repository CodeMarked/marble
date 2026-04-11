#pragma once

#include "MarbleSampleShaderNames.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace marble::game_shared {

inline constexpr std::size_t kMaxMeshSpirvBytes = static_cast<std::size_t>(16) * 1024 * 1024;

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
    if (outVertSpirv.empty() || outFragSpirv.empty() || outFragEmissiveSpirv.empty()) {
        return false;
    }
    if (outVertSpirv.size() % 4 != 0 || outFragSpirv.size() % 4 != 0 || outFragEmissiveSpirv.size() % 4 != 0) {
        return false;
    }
    if (outVertSpirv.size() > kMaxMeshSpirvBytes || outFragSpirv.size() > kMaxMeshSpirvBytes ||
        outFragEmissiveSpirv.size() > kMaxMeshSpirvBytes) {
        return false;
    }
    return true;
}

} // namespace marble::game_shared
