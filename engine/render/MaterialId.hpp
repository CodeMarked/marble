#pragma once

#include <cstddef>
#include <cstdint>

namespace marble::render {

/// Selects a Vulkan graphics pipeline variant (depth/blend fixed function). Must stay in sync with
/// `VulkanRhi` pipeline slot count.
inline constexpr std::uint8_t kMaterialDefault = 0; ///< Depth test on, depth write on (opaque).
inline constexpr std::uint8_t kMaterialTranslucent = 1; ///< Depth test on, depth write off.

/// Number of material slots built in `VulkanRhi::createRenderPassAndPipeline`. Unknown `materialId`
/// values at draw time are treated as `kMaterialDefault` (see `VulkanRhi::drawFrame`).
inline constexpr std::size_t kMaterialPipelineSlotCount = 2u;

} // namespace marble::render
