#pragma once

#include "core/MeshAssetV1.hpp"
#include "math/Vec3.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace marble::core {

/// Matches Jolt `ConvexHullShape::cMaxPointsInHull` (256); core stays Jolt-free.
inline constexpr std::size_t kMeshAssetV1PhysicsConvexHullMaxPoints = 256u;

/// Hard cap on indexed triangles accepted for static mesh collision from MRBMESH1.
inline constexpr std::uint32_t kMeshAssetV1PhysicsMaxTriangles = 1u << 20;

[[nodiscard]] inline bool meshAssetV1ReadPosition(
    MeshAssetV1CpuViews const& views,
    std::uint32_t vertexIndex,
    math::Vec3& out
) noexcept {
    if (vertexIndex >= views.vertexCount) {
        return false;
    }
    std::size_t const off = static_cast<std::size_t>(vertexIndex) * kMeshAssetV1VertexStrideBytes;
    if (views.vertexBytes.size() < off + sizeof(float) * 3u) {
        return false;
    }
    float x{};
    float y{};
    float z{};
    std::memcpy(&x, views.vertexBytes.data() + off, sizeof(float));
    std::memcpy(&y, views.vertexBytes.data() + off + sizeof(float), sizeof(float));
    std::memcpy(&z, views.vertexBytes.data() + off + 2u * sizeof(float), sizeof(float));
    out = math::Vec3{x, y, z};
    return true;
}

/// Appends vertex positions (xyz only) in vertex order; does not clear `out`.
[[nodiscard]] inline bool meshAssetV1AppendPositions(
    MeshAssetV1CpuViews const& views,
    std::vector<math::Vec3>& out
) noexcept {
    for (std::uint32_t vi = 0; vi < views.vertexCount; ++vi) {
        math::Vec3 p{};
        if (!meshAssetV1ReadPosition(views, vi, p)) {
            return false;
        }
        out.push_back(p);
    }
    return true;
}

/// Clears `out`, then fills at most [`kMeshAssetV1PhysicsConvexHullMaxPoints`] samples for Jolt convex hull input.
/// When `vertexCount` exceeds the cap, indices are spaced uniformly in `[0, vertexCount - 1]` (deterministic).
[[nodiscard]] inline bool meshAssetV1ConvexHullSourcePointsResampled(
    MeshAssetV1CpuViews const& views,
    std::vector<math::Vec3>& out
) noexcept {
    out.clear();
    if (views.vertexCount == 0u) {
        return false;
    }
    std::size_t const cap = kMeshAssetV1PhysicsConvexHullMaxPoints;
    if (static_cast<std::size_t>(views.vertexCount) <= cap) {
        return meshAssetV1AppendPositions(views, out);
    }
    std::uint32_t const n = views.vertexCount;
    out.reserve(cap);
    for (std::size_t i = 0; i < cap; ++i) {
        std::uint32_t const idx =
            n <= 1u ? 0u
                    : static_cast<std::uint32_t>((static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(n - 1u)) /
                                                 (cap - 1u));
        math::Vec3 p{};
        if (!meshAssetV1ReadPosition(views, idx, p)) {
            return false;
        }
        out.push_back(p);
    }
    return true;
}

/// Fills `outVerts` and `outIndices` from validated views: full vertex table plus triangle list (`indexCount` must be a multiple of 3).
[[nodiscard]] inline bool meshAssetV1IndexedTriangleMeshForPhysics(
    MeshAssetV1CpuViews const& views,
    std::vector<math::Vec3>& outVerts,
    std::vector<std::uint32_t>& outIndices
) noexcept {
    outVerts.clear();
    outIndices.clear();
    if (views.vertexCount == 0u || views.indexCount == 0u) {
        return false;
    }
    if (views.indexCount % 3u != 0u) {
        return false;
    }
    std::uint32_t const triCount = views.indexCount / 3u;
    if (triCount > kMeshAssetV1PhysicsMaxTriangles) {
        return false;
    }
    if (!meshAssetV1AppendPositions(views, outVerts)) {
        return false;
    }
    outIndices.resize(static_cast<std::size_t>(views.indexCount));
    for (std::uint32_t ii = 0; ii < views.indexCount; ++ii) {
        std::optional<std::uint32_t> const w = meshAssetV1IndexAt(views, ii);
        if (!w.has_value()) {
            return false;
        }
        outIndices[static_cast<std::size_t>(ii)] = *w;
    }
    return true;
}

} // namespace marble::core
