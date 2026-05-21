#pragma once

#include "core/MeshAssetV1.hpp"
#include "core/MeshAssetV1PhysicsExtract.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "math/Mat4.hpp"
#include "math/Vec3.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace marble::core {

/// Bakes MRBMESH1 template (vertices in unit `[-0.5,0.5]³`) into gameplay-world positions for Garden static props.
/// World vertex: `center + R(ypr) * (2*halfExtents ⊙ pTemplate)`. Indices and trailing vertex fields are preserved.
[[nodiscard]] inline bool meshAssetV1BakeGardenPropInstanceToWorldBytes(
    std::span<std::uint8_t const> templateBytes,
    math::Vec3 const& center,
    math::Vec3 const& halfExtents,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    std::vector<std::uint8_t>& out
) noexcept {
    std::optional<MeshAssetV1CpuViews> const parsed = meshAssetV1TryParse(templateBytes);
    if (!parsed.has_value() || parsed->vertexCount == 0u) {
        return false;
    }
    MeshAssetV1CpuViews const& v = *parsed;
    math::Mat4 const r =
        math::Mat4::rotationY(yawRadians) * math::Mat4::rotationX(pitchRadians) * math::Mat4::rotationZ(rollRadians);

    out.clear();
    out.reserve(templateBytes.size());
    appendMeshAssetV1Header(out, kMeshAssetV1Version1, v.vertexCount, v.indexCount);
    for (std::uint32_t vi = 0; vi < v.vertexCount; ++vi) {
        math::Vec3 pUnit{};
        if (!meshAssetV1ReadPosition(v, vi, pUnit)) {
            return false;
        }
        math::Vec3 const scaled{
            2.f * halfExtents.x * pUnit.x,
            2.f * halfExtents.y * pUnit.y,
            2.f * halfExtents.z * pUnit.z,
        };
        math::Vec3 const w = center + math::transformDirection(r, scaled);
        if (!std::isfinite(w.x) || !std::isfinite(w.y) || !std::isfinite(w.z)) {
            return false;
        }
        std::size_t const srcBase = static_cast<std::size_t>(vi) * kMeshAssetV1VertexStrideBytes;
        if (v.vertexBytes.size() < srcBase + kMeshAssetV1VertexStrideBytes) {
            return false;
        }
        mesh_asset_v1_write_detail::appendLeF32(out, w.x);
        mesh_asset_v1_write_detail::appendLeF32(out, w.y);
        mesh_asset_v1_write_detail::appendLeF32(out, w.z);
        out.insert(
            out.end(),
            v.vertexBytes.data() + srcBase + 3u * sizeof(float),
            v.vertexBytes.data() + srcBase + kMeshAssetV1VertexStrideBytes);
    }
    out.insert(out.end(), v.indexBytes.begin(), v.indexBytes.end());
    return meshAssetV1Valid(std::span<std::uint8_t const>(out.data(), out.size()));
}

} // namespace marble::core
