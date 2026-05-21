#pragma once

#include "core/MeshAssetV1.hpp"

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace marble::core {

namespace mesh_asset_v1_write_detail {

inline void appendLeU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 24u) & 0xFFu));
}

inline void appendLeF32(std::vector<std::uint8_t>& out, float f) {
    appendLeU32(out, std::bit_cast<std::uint32_t>(f));
}

} // namespace mesh_asset_v1_write_detail

/// Append MRBMESH1 v1 header (32 bytes): magic, `version`, `vertexCount`, `indexCount`, three reserved `u32` (default 0).
inline void appendMeshAssetV1Header(
    std::vector<std::uint8_t>& out,
    std::uint32_t version,
    std::uint32_t vertexCount,
    std::uint32_t indexCount,
    std::uint32_t reserved0 = 0u,
    std::uint32_t reserved1 = 0u,
    std::uint32_t reserved2 = 0u
) {
    using mesh_asset_v1_write_detail::appendLeU32;
    static constexpr char kMagic[8] = {'M', 'R', 'B', 'M', 'E', 'S', 'H', '1'};
    out.insert(out.end(), std::begin(kMagic), std::end(kMagic));
    appendLeU32(out, version);
    appendLeU32(out, vertexCount);
    appendLeU32(out, indexCount);
    appendLeU32(out, reserved0);
    appendLeU32(out, reserved1);
    appendLeU32(out, reserved2);
}

/// One demo vertex: origin, +Z normal, white — matches [`meshAssetV1TryParse`](MeshAssetV1.hpp) finite checks.
inline void appendMeshAssetV1DemoVertex(std::vector<std::uint8_t>& out) {
    using mesh_asset_v1_write_detail::appendLeF32;
    appendLeF32(out, 0.f);
    appendLeF32(out, 0.f);
    appendLeF32(out, 0.f);
    appendLeF32(out, 0.f);
    appendLeF32(out, 0.f);
    appendLeF32(out, 1.f);
    appendLeF32(out, 1.f);
    appendLeF32(out, 1.f);
    appendLeF32(out, 1.f);
}

/// ADR-0063-valid single triangle: 3 vertices × 9 floats, indices 0,1,2.
/// One vertex with full MRBMESH1 layout (position, normal, color).
inline void appendMeshAssetV1Vertex(
    std::vector<std::uint8_t>& out,
    float px,
    float py,
    float pz,
    float nx,
    float ny,
    float nz,
    float cr,
    float cg,
    float cb
) {
    using mesh_asset_v1_write_detail::appendLeF32;
    appendLeF32(out, px);
    appendLeF32(out, py);
    appendLeF32(out, pz);
    appendLeF32(out, nx);
    appendLeF32(out, ny);
    appendLeF32(out, nz);
    appendLeF32(out, cr);
    appendLeF32(out, cg);
    appendLeF32(out, cb);
}

/// Unit-cube ramp: quad in XZ, rises toward +Z in +Y; fits `[-0.5,0.5]³`, CCW from +Y.
[[nodiscard]] inline std::vector<std::uint8_t> meshAssetV1BuildGardenTiltedRampTemplateBytes() {
    using mesh_asset_v1_write_detail::appendLeU32;
    std::vector<std::uint8_t> out;
    appendMeshAssetV1Header(out, kMeshAssetV1Version1, 4u, 6u);
    appendMeshAssetV1Vertex(out, -0.5f, 0.f, -0.5f, 0.f, 1.f, 0.f, 0.75f, 0.72f, 0.55f);
    appendMeshAssetV1Vertex(out, 0.5f, 0.f, -0.5f, 0.f, 1.f, 0.f, 0.75f, 0.72f, 0.55f);
    appendMeshAssetV1Vertex(out, 0.5f, 0.16f, 0.45f, 0.f, 1.f, 0.f, 0.75f, 0.72f, 0.55f);
    appendMeshAssetV1Vertex(out, -0.5f, 0.16f, 0.45f, 0.f, 1.f, 0.f, 0.75f, 0.72f, 0.55f);
    appendLeU32(out, 0u);
    appendLeU32(out, 3u);
    appendLeU32(out, 1u);
    appendLeU32(out, 1u);
    appendLeU32(out, 3u);
    appendLeU32(out, 2u);
    return out;
}

/// Pallet in unit `[-0.5,0.5]³`: four solid deck boards + two skid beams (axis-aligned boxes). Face winding matches
/// `addCube` in `ProceduralMeshVulkan.hpp` for consistent outward normals (lit mesh + physics).
[[nodiscard]] inline std::vector<std::uint8_t> meshAssetV1BuildGardenBenchSlatsTemplateBytes() {
    using mesh_asset_v1_write_detail::appendLeU32;
    std::vector<std::uint8_t> vb;
    std::vector<std::uint8_t> ib;
    std::uint32_t vcount = 0;

    auto face = [&](float nx,
        float ny,
        float nz,
        float ax,
        float ay,
        float az,
        float bx,
        float by,
        float bz,
        float cx,
        float cy,
        float cz,
        float dx,
        float dy,
        float dz,
        float cr,
        float cg,
        float cb) {
        std::uint32_t const base = vcount;
        appendMeshAssetV1Vertex(vb, ax, ay, az, nx, ny, nz, cr, cg, cb);
        appendMeshAssetV1Vertex(vb, bx, by, bz, nx, ny, nz, cr, cg, cb);
        appendMeshAssetV1Vertex(vb, cx, cy, cz, nx, ny, nz, cr, cg, cb);
        appendMeshAssetV1Vertex(vb, dx, dy, dz, nx, ny, nz, cr, cg, cb);
        appendLeU32(ib, base);
        appendLeU32(ib, base + 1u);
        appendLeU32(ib, base + 2u);
        appendLeU32(ib, base + 2u);
        appendLeU32(ib, base + 3u);
        appendLeU32(ib, base);
        vcount += 4u;
    };

    auto box = [&](float x0, float x1, float y0, float y1, float z0, float z1, float cr, float cg, float cb) {
        // Same corner order as `addCube` (±h box), generalized to [x0,x1]×[y0,y1]×[z0,z1].
        face(1.f, 0.f, 0.f, x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1, cr, cg, cb);
        face(-1.f, 0.f, 0.f, x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0, cr, cg, cb);
        face(0.f, 1.f, 0.f, x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, cr, cg, cb);
        face(0.f, -1.f, 0.f, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, cr, cg, cb);
        face(0.f, 0.f, 1.f, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, cr, cg, cb);
        face(0.f, 0.f, -1.f, x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, cr, cg, cb);
    };

    float constexpr cr = 0.42f;
    float constexpr cg = 0.28f;
    float constexpr cb = 0.14f;
    float constexpr xl = -0.44f;
    float constexpr xr = 0.44f;
    float constexpr yDeckLo = 0.056f;
    float constexpr yDeckHi = 0.092f;
    struct ZBand {
        float z0;
        float z1;
    };
    ZBand const boards[4] = {
        {-0.45f, -0.30f},
        {-0.26f, -0.11f},
        {-0.07f, 0.08f},
        {0.12f, 0.45f},
    };
    for (ZBand b : boards) {
        box(xl, xr, yDeckLo, yDeckHi, b.z0, b.z1, cr * 0.96f, cg * 0.96f, cb * 0.96f);
    }

    float constexpr ySkidBot = -0.38f;
    float constexpr ySkidTop = 0.056f;
    float constexpr zSkid0 = -0.40f;
    float constexpr zSkid1 = 0.40f;
    box(-0.395f, -0.305f, ySkidBot, ySkidTop, zSkid0, zSkid1, cr * 0.88f, cg * 0.88f, cb * 0.88f);
    box(0.305f, 0.395f, ySkidBot, ySkidTop, zSkid0, zSkid1, cr * 0.88f, cg * 0.88f, cb * 0.88f);

    std::uint32_t const vcountFinal = static_cast<std::uint32_t>(vb.size() / kMeshAssetV1VertexStrideBytes);
    std::uint32_t const icountFinal = static_cast<std::uint32_t>(ib.size() / 4u);
    std::vector<std::uint8_t> out;
    appendMeshAssetV1Header(out, kMeshAssetV1Version1, vcountFinal, icountFinal);
    out.insert(out.end(), vb.begin(), vb.end());
    out.insert(out.end(), ib.begin(), ib.end());
    return out;
}

/// Two-triangle quad on XZ at y=0 (legacy demo scale 2×1); not normalized to unit cube.
[[nodiscard]] inline std::vector<std::uint8_t> meshAssetV1BuildDemoRampBytes() {
    using mesh_asset_v1_write_detail::appendLeU32;
    std::vector<std::uint8_t> out;
    appendMeshAssetV1Header(out, kMeshAssetV1Version1, 4u, 6u);
    appendMeshAssetV1Vertex(out, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 1.f);
    appendMeshAssetV1Vertex(out, 2.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 1.f);
    appendMeshAssetV1Vertex(out, 2.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 1.f, 1.f);
    appendMeshAssetV1Vertex(out, 0.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 1.f, 1.f);
    appendLeU32(out, 0u);
    appendLeU32(out, 3u);
    appendLeU32(out, 1u);
    appendLeU32(out, 1u);
    appendLeU32(out, 3u);
    appendLeU32(out, 2u);
    return out;
}

[[nodiscard]] inline std::vector<std::uint8_t> meshAssetV1BuildDemoTriangleBytes() {
    using mesh_asset_v1_write_detail::appendLeU32;
    std::vector<std::uint8_t> out;
    appendMeshAssetV1Header(out, kMeshAssetV1Version1, 3u, 3u);
    appendMeshAssetV1DemoVertex(out);
    appendMeshAssetV1DemoVertex(out);
    appendMeshAssetV1DemoVertex(out);
    appendLeU32(out, 0u);
    appendLeU32(out, 1u);
    appendLeU32(out, 2u);
    return out;
}

[[nodiscard]] inline std::vector<std::uint8_t> meshAssetV1BuildEmptyV1Bytes() {
    std::vector<std::uint8_t> out;
    appendMeshAssetV1Header(out, kMeshAssetV1Version1, 0u, 0u);
    return out;
}

[[nodiscard]] inline bool writeMeshAssetV1BytesToFile(std::filesystem::path const& path, std::span<std::uint8_t const> bytes) {
    if (path.empty()) {
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

[[nodiscard]] inline bool writeMeshAssetV1DemoTriangleFile(std::filesystem::path const& path) {
    std::vector<std::uint8_t> const b = meshAssetV1BuildDemoTriangleBytes();
    return writeMeshAssetV1BytesToFile(path, std::span<std::uint8_t const>(b.data(), b.size()));
}

} // namespace marble::core
