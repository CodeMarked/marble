#pragma once

#include <cstdint>

namespace marble::render {

/// Push constant `flags` bits (vertex + fragment); must match the sample mesh GLSL layout (stems from
/// `MARBLE_SHADER_GLSL_MODULES` / `game/marbles/shaders/`; order contract in ADR-0057).
inline constexpr std::uint32_t kPcFlagClipSpace = 1u; ///< Vertex.xy are NDC; skip viewProj.
inline constexpr std::uint32_t kPcFlagUnlit = 2u; ///< Fragment: no lighting or fog.
/// `VulkanRhi` selects the emissive mesh fragment pipeline when SPIR-V was provided at init.
inline constexpr std::uint32_t kPcFlagMeshEmissive = 4u;

} // namespace marble::render
