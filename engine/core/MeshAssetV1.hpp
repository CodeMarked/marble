#pragma once

#include <bit>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace marble::core {

/// Marble mesh interchange **v1** (typed `ResourceKind::Mesh`): decode-only, strict validation.
///
/// Layout (all integers **little-endian**):
/// - Bytes `[0,8)` ASCII magic **`MRBMESH1`** (exactly eight bytes, no NUL).
/// - Bytes `[8,12)` `u32` **version**; only **1** is accepted.
/// - Bytes `[12,16)` `u32` **vertexCount** (number of vertices).
/// - Bytes `[16,20)` `u32` **indexCount** (number of `u32` indices, not bytes).
/// - Bytes `[20,32)` three reserved `u32` words; all must be **0** in v1.
/// - Then **`vertexCount * kMeshAssetV1VertexStrideBytes`** vertex bytes.
/// - Then **`indexCount * sizeof(u32)`** index bytes (each index little-endian).
///
/// Each vertex is **9** IEEE-754 **binary32** values in order: position xyz, normal xyz, color rgb.
/// Stride is **36** bytes (must match `marble::render::VulkanRhi::Vertex` layout at the engine/RHI boundary).
///
/// **Well-formedness:** `vertexCount` and `indexCount` may both be zero. If `vertexCount == 0`, `indexCount`
/// must be zero. Otherwise every index must satisfy `index < vertexCount`. All decoded floats must be finite
/// (`std::isfinite`). Total file size must equal the header plus payload exactly (no slack, no padding).

inline constexpr std::string_view kMeshAssetV1Magic = "MRBMESH1";
inline constexpr std::uint32_t kMeshAssetV1Version1 = 1u;
inline constexpr std::size_t kMeshAssetV1HeaderBytes = 32u;
inline constexpr std::size_t kMeshAssetV1FloatsPerVertex = 9u;
inline constexpr std::size_t kMeshAssetV1VertexStrideBytes = kMeshAssetV1FloatsPerVertex * sizeof(float);
inline constexpr std::uint32_t kMeshAssetV1MaxVertices = 1u << 22; // 4 Mi verts — hard cap for v1
inline constexpr std::uint32_t kMeshAssetV1MaxIndices = 1u << 26; // index count cap (total bytes bounded below)
inline constexpr std::uint64_t kMeshAssetV1MaxTotalBytes =
    (std::uint64_t{64} * 1024u * 1024u); // 64 MiB entire file including header

struct MeshAssetV1CpuViews {
    std::uint32_t vertexCount{0};
    std::uint32_t indexCount{0};
    /// `vertexCount * kMeshAssetV1VertexStrideBytes` bytes; empty when `vertexCount == 0`.
    std::span<std::uint8_t const> vertexBytes{};
    /// `indexCount * 4` bytes, little-endian `u32` per index; empty when `indexCount == 0`.
    std::span<std::uint8_t const> indexBytes{};
};

namespace mesh_asset_v1_detail {

[[nodiscard]] inline bool readLeU32(std::span<std::uint8_t const> bytes, std::size_t offset, std::uint32_t& out) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        return false;
    }
    std::uint8_t const* const p = bytes.data() + offset;
    out = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8u) |
          (static_cast<std::uint32_t>(p[2]) << 16u) | (static_cast<std::uint32_t>(p[3]) << 24u);
    return true;
}

[[nodiscard]] inline bool finiteLeF32(std::span<std::uint8_t const> bytes, std::size_t offset) noexcept {
    std::uint32_t u{};
    if (!readLeU32(bytes, offset, u)) {
        return false;
    }
    float const f = std::bit_cast<float>(u);
    return std::isfinite(f);
}

} // namespace mesh_asset_v1_detail

/// Full structural + numeric validation for v1 mesh blobs (no GPU upload).
[[nodiscard]] inline std::optional<MeshAssetV1CpuViews> meshAssetV1TryParse(std::span<std::uint8_t const> bytes) noexcept {
    using mesh_asset_v1_detail::finiteLeF32;
    using mesh_asset_v1_detail::readLeU32;

    if (bytes.size() < kMeshAssetV1HeaderBytes) {
        return std::nullopt;
    }
    if (bytes.size() > static_cast<std::size_t>(kMeshAssetV1MaxTotalBytes)) {
        return std::nullopt;
    }
    if (bytes.size() < kMeshAssetV1Magic.size() ||
        !std::equal(kMeshAssetV1Magic.begin(), kMeshAssetV1Magic.end(), bytes.begin())) {
        return std::nullopt;
    }
    std::uint32_t version{};
    std::uint32_t vertexCount{};
    std::uint32_t indexCount{};
    std::uint32_t r0{};
    std::uint32_t r1{};
    std::uint32_t r2{};
    if (!readLeU32(bytes, 8u, version) || !readLeU32(bytes, 12u, vertexCount) || !readLeU32(bytes, 16u, indexCount) ||
        !readLeU32(bytes, 20u, r0) || !readLeU32(bytes, 24u, r1) || !readLeU32(bytes, 28u, r2)) {
        return std::nullopt;
    }
    if (version != kMeshAssetV1Version1 || r0 != 0u || r1 != 0u || r2 != 0u) {
        return std::nullopt;
    }
    if (vertexCount > kMeshAssetV1MaxVertices || indexCount > kMeshAssetV1MaxIndices) {
        return std::nullopt;
    }
    if (vertexCount == 0u) {
        if (indexCount != 0u) {
            return std::nullopt;
        }
        if (bytes.size() != kMeshAssetV1HeaderBytes) {
            return std::nullopt;
        }
        return MeshAssetV1CpuViews{0u, 0u, {}, {}};
    }

    std::uint64_t const vertexPayload =
        static_cast<std::uint64_t>(vertexCount) * static_cast<std::uint64_t>(kMeshAssetV1VertexStrideBytes);
    std::uint64_t const indexPayload = static_cast<std::uint64_t>(indexCount) * 4u;
    std::uint64_t const body = vertexPayload + indexPayload;
    if (body > kMeshAssetV1MaxTotalBytes - kMeshAssetV1HeaderBytes) {
        return std::nullopt;
    }
    if (body > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - kMeshAssetV1HeaderBytes)) {
        return std::nullopt;
    }
    std::size_t const totalSize = kMeshAssetV1HeaderBytes + static_cast<std::size_t>(body);
    if (totalSize != bytes.size()) {
        return std::nullopt;
    }

    std::size_t const vertexBegin = kMeshAssetV1HeaderBytes;
    std::size_t const indexBegin = vertexBegin + static_cast<std::size_t>(vertexPayload);
    for (std::uint32_t v = 0; v < vertexCount; ++v) {
        std::size_t const base = vertexBegin + static_cast<std::size_t>(v) * kMeshAssetV1VertexStrideBytes;
        for (std::size_t f = 0; f < kMeshAssetV1FloatsPerVertex; ++f) {
            if (!finiteLeF32(bytes, base + f * sizeof(float))) {
                return std::nullopt;
            }
        }
    }

    for (std::uint32_t i = 0; i < indexCount; ++i) {
        std::uint32_t idx{};
        if (!readLeU32(bytes, indexBegin + static_cast<std::size_t>(i) * 4u, idx)) {
            return std::nullopt;
        }
        if (idx >= vertexCount) {
            return std::nullopt;
        }
    }

    std::span<std::uint8_t const> const vb = bytes.subspan(vertexBegin, static_cast<std::size_t>(vertexPayload));
    std::span<std::uint8_t const> const ib =
        indexCount > 0u ? bytes.subspan(indexBegin, static_cast<std::size_t>(indexCount) * 4u)
                        : std::span<std::uint8_t const>{};
    return MeshAssetV1CpuViews{vertexCount, indexCount, vb, ib};
}

[[nodiscard]] inline bool meshAssetV1Valid(std::span<std::uint8_t const> bytes) noexcept {
    return meshAssetV1TryParse(bytes).has_value();
}

/// Read one index value from validated `indexBytes` (LE `u32` at `which * 4`).
[[nodiscard]] inline std::optional<std::uint32_t> meshAssetV1IndexAt(MeshAssetV1CpuViews const& v, std::uint32_t which) noexcept {
    if (which >= v.indexCount) {
        return std::nullopt;
    }
    std::uint32_t out{};
    if (!mesh_asset_v1_detail::readLeU32(v.indexBytes, static_cast<std::size_t>(which) * 4u, out)) {
        return std::nullopt;
    }
    return out;
}

} // namespace marble::core
