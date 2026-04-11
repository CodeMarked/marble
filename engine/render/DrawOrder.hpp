#pragma once

#include "render/RenderTypes.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <span>

namespace marble::render {

/// Stable sort indices so draws are ordered by `(drawLayer, materialId, meshIndex)` for fewer
/// pipeline binds and deterministic transparency ordering. `indicesOut.size()` must equal `draws.size()`.
inline void sortMeshDrawInstanceIndices(std::span<MeshDrawInstance const> draws, std::span<std::uint32_t> indicesOut) {
    if (draws.empty()) {
        return;
    }
    std::iota(indicesOut.begin(), indicesOut.end(), 0u);
    std::stable_sort(indicesOut.begin(), indicesOut.end(), [&](std::uint32_t i, std::uint32_t j) {
        MeshDrawInstance const& a = draws[i];
        MeshDrawInstance const& b = draws[j];
        if (a.drawLayer != b.drawLayer) {
            return a.drawLayer < b.drawLayer;
        }
        if (a.materialId != b.materialId) {
            return a.materialId < b.materialId;
        }
        if (a.meshIndex != b.meshIndex) {
            return a.meshIndex < b.meshIndex;
        }
        return i < j;
    });
}

} // namespace marble::render
