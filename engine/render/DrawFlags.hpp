#pragma once

#include <cstdint>

namespace marble::render {

/// Push constant `flags` bits (vertex + fragment); must match `mesh.vert` / `mesh.frag`.
inline constexpr std::uint32_t kPcFlagClipSpace = 1u; ///< Vertex.xy are NDC; skip viewProj.
inline constexpr std::uint32_t kPcFlagUnlit = 2u; ///< Fragment: no lighting or fog.

} // namespace marble::render
