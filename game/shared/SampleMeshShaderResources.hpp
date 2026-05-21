#pragma once

#include "MarbleSampleShaderNames.hpp"

#include "core/ResourceManager.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace marble::game_shared {

/// If `assetsRoot/kMarbleSampleMeshShadersPackFileName` exists, attach it for loose-then-pack `acquire` (ADR-0024).
/// Call after `setBinaryResourceSearchRoot` / `setSearchRoots`. Returns whether a pack was opened.
template <std::size_t Capacity>
[[nodiscard]] inline bool tryAttachSampleMeshShadersPack(
    marble::core::BinaryResourceManager<Capacity>& rm,
    std::string const& assetsRoot
) {
    if (assetsRoot.empty()) {
        return false;
    }
    std::filesystem::path const packPath =
        std::filesystem::path(assetsRoot) / std::string{kMarbleSampleMeshShadersPackFileName};
    std::error_code ec;
    if (!std::filesystem::is_regular_file(packPath, ec) || ec) {
        return false;
    }
    rm.setReadOnlyPack(packPath);
    return true;
}

template <std::size_t Capacity>
[[nodiscard]] inline bool acquireAllSampleMeshRegistryShaders(marble::core::BinaryResourceManager<Capacity>& rm) {
    for (std::size_t i = 0; i < kMarbleSampleShaderCount; ++i) {
        if (!rm.acquire(kMarbleSampleShaderRegistryPaths[i], marble::core::ResourceKind::ShaderBytecode)) {
            return false;
        }
    }
    return true;
}

/// Fills spans for `VulkanRhi::initFromSpirvBytes` (vert, lit frag, emissive frag) from registry paths in list order.
template <std::size_t Capacity>
[[nodiscard]] inline bool sampleMeshRegistrySpirvSpansForInit(
    marble::core::BinaryResourceManager<Capacity> const& rm,
    std::span<std::uint8_t const>& outVert,
    std::span<std::uint8_t const>& outFragLit,
    std::span<std::uint8_t const>& outFragEmissive
) {
    static_assert(kMarbleSampleShaderCount == 3, "update span wiring when sample shader count changes");
    marble::core::BinaryResource const* const v = rm.find(kMarbleSampleShaderRegistryPaths[0]);
    marble::core::BinaryResource const* const f = rm.find(kMarbleSampleShaderRegistryPaths[1]);
    marble::core::BinaryResource const* const e = rm.find(kMarbleSampleShaderRegistryPaths[2]);
    if (v == nullptr || f == nullptr || e == nullptr) {
        return false;
    }
    std::optional<std::span<std::uint8_t const>> const ov = marble::core::shaderBytecodeSpanFrom(v);
    std::optional<std::span<std::uint8_t const>> const of = marble::core::shaderBytecodeSpanFrom(f);
    std::optional<std::span<std::uint8_t const>> const oe = marble::core::shaderBytecodeSpanFrom(e);
    if (!ov.has_value() || !of.has_value() || !oe.has_value()) {
        return false;
    }
    outVert = *ov;
    outFragLit = *of;
    outFragEmissive = *oe;
    return true;
}

} // namespace marble::game_shared
