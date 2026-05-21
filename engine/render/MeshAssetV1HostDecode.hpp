#pragma once

#include "core/MeshAssetV1.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace marble::render {

/// Decode validated `marble::core::MeshAssetV1CpuViews` into host buffers for `VulkanRhi::uploadMesh`. Callers should
/// pass views only from `meshAssetV1TryParse` /
/// `meshAssetV1ViewsFrom`. Returns false for empty geometry (upload path requires non-empty verts and indices).
[[nodiscard]] inline bool meshAssetV1CpuViewsToHostBuffers(
    marble::core::MeshAssetV1CpuViews const& views,
    std::vector<VulkanRhi::Vertex>& outVertices,
    std::vector<std::uint32_t>& outIndices
) noexcept {
    if (views.vertexCount == 0u || views.indexCount == 0u) {
        return false;
    }
    std::size_t const expectedVb =
        static_cast<std::size_t>(views.vertexCount) * marble::core::kMeshAssetV1VertexStrideBytes;
    if (views.vertexBytes.size() != expectedVb) {
        return false;
    }
    std::size_t const expectedIb = static_cast<std::size_t>(views.indexCount) * 4u;
    if (views.indexBytes.size() != expectedIb) {
        return false;
    }

    outVertices.resize(static_cast<std::size_t>(views.vertexCount));
    for (std::uint32_t vi = 0; vi < views.vertexCount; ++vi) {
        std::size_t const off = static_cast<std::size_t>(vi) * marble::core::kMeshAssetV1VertexStrideBytes;
        std::memcpy(&outVertices[vi], views.vertexBytes.data() + off, sizeof(VulkanRhi::Vertex));
    }

    outIndices.resize(static_cast<std::size_t>(views.indexCount));
    for (std::uint32_t ii = 0; ii < views.indexCount; ++ii) {
        std::optional<std::uint32_t> const w = marble::core::meshAssetV1IndexAt(views, ii);
        if (!w.has_value()) {
            return false;
        }
        outIndices[ii] = *w;
    }
    return true;
}

} // namespace marble::render
