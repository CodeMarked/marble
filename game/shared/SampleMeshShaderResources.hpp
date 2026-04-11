#pragma once

#include "MarbleSampleShaderNames.hpp"

#include "core/ResourceManager.hpp"

#include <cstddef>
#include <span>

namespace marble::game_shared {

template <std::size_t Capacity>
[[nodiscard]] inline bool acquireAllSampleMeshRegistryShaders(marble::core::BinaryResourceManager<Capacity>& rm) {
    for (std::size_t i = 0; i < kMarbleSampleShaderCount; ++i) {
        if (!rm.acquire(kMarbleSampleShaderRegistryPaths[i])) {
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
    outVert = {v->bytes.data(), v->bytes.size()};
    outFragLit = {f->bytes.data(), f->bytes.size()};
    outFragEmissive = {e->bytes.data(), e->bytes.size()};
    return true;
}

} // namespace marble::game_shared
