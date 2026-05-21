#include "core/MeshAssetV1.hpp"
#include "core/MeshAssetV1GardenBake.hpp"
#include "core/MeshAssetV1PhysicsExtract.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "core/ResourceManager.hpp"
#include "math/Vec3.hpp"

#include <cmath>
#include "render/vulkan/VulkanRhi.hpp"

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <vector>

int main() {
    static_assert(sizeof(marble::render::VulkanRhi::Vertex) == marble::core::kMeshAssetV1VertexStrideBytes);

    std::vector<std::uint8_t> const emptyOk = marble::core::meshAssetV1BuildEmptyV1Bytes();
    if (!marble::core::meshAssetV1Valid(std::span(emptyOk)) || !marble::core::meshAssetV1TryParse(std::span(emptyOk)).has_value()) {
        return 1;
    }

    std::vector<std::uint8_t> const tri = marble::core::meshAssetV1BuildDemoTriangleBytes();
    {
        std::optional<marble::core::MeshAssetV1CpuViews> const v = marble::core::meshAssetV1TryParse(std::span(tri));
        if (!v.has_value() || v->vertexCount != 3u || v->indexCount != 3u || v->vertexBytes.size() != 3u * 36u ||
            v->indexBytes.size() != 12u) {
            return 2;
        }
        if (marble::core::meshAssetV1IndexAt(*v, 0).value_or(99u) != 0u || marble::core::meshAssetV1IndexAt(*v, 1).value_or(99u) != 1u ||
            marble::core::meshAssetV1IndexAt(*v, 2).value_or(99u) != 2u) {
            return 3;
        }
        if (marble::core::meshAssetV1IndexAt(*v, 3).has_value()) {
            return 4;
        }
    }

    {
        std::vector<std::uint8_t> badMagic = emptyOk;
        badMagic[0] = 'X';
        if (marble::core::meshAssetV1Valid(std::span(badMagic))) {
            return 5;
        }
    }
    {
        std::vector<std::uint8_t> badVer = emptyOk;
        badVer[8] = 2;
        if (marble::core::meshAssetV1Valid(std::span(badVer))) {
            return 6;
        }
    }
    {
        std::vector<std::uint8_t> badRes = emptyOk;
        badRes[20] = 1;
        if (marble::core::meshAssetV1Valid(std::span(badRes))) {
            return 7;
        }
    }
    {
        std::vector<std::uint8_t> zeroVertsNonzeroIdx = emptyOk;
        zeroVertsNonzeroIdx[16] = 1;
        if (marble::core::meshAssetV1Valid(std::span(zeroVertsNonzeroIdx))) {
            return 8;
        }
    }
    {
        std::vector<std::uint8_t> shortFile = tri;
        shortFile.pop_back();
        if (marble::core::meshAssetV1Valid(std::span(shortFile))) {
            return 9;
        }
    }
    {
        std::vector<std::uint8_t> longFile = tri;
        longFile.push_back(0);
        if (marble::core::meshAssetV1Valid(std::span(longFile))) {
            return 10;
        }
    }
    {
        std::vector<std::uint8_t> badIdx = tri;
        std::size_t const off = marble::core::kMeshAssetV1HeaderBytes + 3u * marble::core::kMeshAssetV1VertexStrideBytes;
        badIdx[off + 8u] = 9;
        if (marble::core::meshAssetV1Valid(std::span(badIdx))) {
            return 11;
        }
    }
    {
        std::vector<std::uint8_t> nanVert = tri;
        std::size_t const nanOff = marble::core::kMeshAssetV1HeaderBytes + 4u;
        std::uint32_t const nanWord = std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN());
        nanVert[nanOff + 0u] = static_cast<std::uint8_t>(nanWord & 0xFFu);
        nanVert[nanOff + 1u] = static_cast<std::uint8_t>((nanWord >> 8u) & 0xFFu);
        nanVert[nanOff + 2u] = static_cast<std::uint8_t>((nanWord >> 16u) & 0xFFu);
        nanVert[nanOff + 3u] = static_cast<std::uint8_t>((nanWord >> 24u) & 0xFFu);
        if (marble::core::meshAssetV1Valid(std::span(nanVert))) {
            return 12;
        }
    }
    {
        std::vector<std::uint8_t> infVert = tri;
        std::size_t const infOff = marble::core::kMeshAssetV1HeaderBytes + 4u;
        std::uint32_t const infWord = std::bit_cast<std::uint32_t>(std::numeric_limits<float>::infinity());
        infVert[infOff + 0u] = static_cast<std::uint8_t>(infWord & 0xFFu);
        infVert[infOff + 1u] = static_cast<std::uint8_t>((infWord >> 8u) & 0xFFu);
        infVert[infOff + 2u] = static_cast<std::uint8_t>((infWord >> 16u) & 0xFFu);
        infVert[infOff + 3u] = static_cast<std::uint8_t>((infWord >> 24u) & 0xFFu);
        if (marble::core::meshAssetV1Valid(std::span(infVert))) {
            return 13;
        }
    }
    {
        std::vector<std::uint8_t> huge;
        marble::core::appendMeshAssetV1Header(
            huge, marble::core::kMeshAssetV1Version1, marble::core::kMeshAssetV1MaxVertices + 1u, 0u);
        if (marble::core::meshAssetV1Valid(std::span(huge))) {
            return 14;
        }
    }
    {
        namespace stdfs = std::filesystem;
        stdfs::path const root = stdfs::temp_directory_path() / "marble_mesh_asset_rm_test";
        std::error_code ec;
        stdfs::create_directories(root, ec);
        stdfs::path const meshPath = root / "tri.mesh";
        {
            std::vector<std::uint8_t> const bytes = marble::core::meshAssetV1BuildDemoTriangleBytes();
            std::ofstream out(meshPath, std::ios::binary);
            out.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        marble::core::BinaryResourceManager<16> rm;
        rm.setSearchRoots({root});
        if (!rm.acquire("tri.mesh", marble::core::ResourceKind::Mesh)) {
            stdfs::remove_all(root, ec);
            return 15;
        }
        marble::core::BinaryResource const* r = rm.find("tri.mesh");
        if (r == nullptr || r->category != marble::core::ResourceKind::Mesh) {
            stdfs::remove_all(root, ec);
            return 16;
        }
        std::optional<marble::core::MeshAssetV1CpuViews> const views = marble::core::meshAssetV1ViewsFrom(r);
        if (!views.has_value() || views->vertexCount != 3u) {
            stdfs::remove_all(root, ec);
            return 17;
        }
        if (marble::core::shaderBytecodeSpanFrom(r).has_value()) {
            stdfs::remove_all(root, ec);
            return 18;
        }
        {
            std::vector<std::uint8_t> bad = marble::core::meshAssetV1BuildDemoTriangleBytes();
            bad[bad.size() - 1] ^= 0xFFu;
            std::ofstream out(root / "bad.mesh", std::ios::binary);
            out.write(reinterpret_cast<char const*>(bad.data()), static_cast<std::streamsize>(bad.size()));
        }
        if (rm.acquire("bad.mesh", marble::core::ResourceKind::Mesh) || rm.loadedCount() != 1) {
            stdfs::remove_all(root, ec);
            return 19;
        }
        if (!rm.release("tri.mesh") || rm.loadedCount() != 0) {
            stdfs::remove_all(root, ec);
            return 20;
        }
        stdfs::remove_all(root, ec);
    }

    {
        std::vector<std::uint8_t> const ramp = marble::core::meshAssetV1BuildDemoRampBytes();
        if (!marble::core::meshAssetV1Valid(std::span(ramp))) {
            return 21;
        }
        std::optional<marble::core::MeshAssetV1CpuViews> const rv = marble::core::meshAssetV1TryParse(std::span(ramp));
        if (!rv.has_value() || rv->vertexCount != 4u || rv->indexCount != 6u) {
            return 22;
        }
        std::vector<marble::math::Vec3> vbuf;
        std::vector<std::uint32_t> ibuf;
        if (!marble::core::meshAssetV1IndexedTriangleMeshForPhysics(*rv, vbuf, ibuf) || vbuf.size() != 4u || ibuf.size() != 6u) {
            return 23;
        }
    }

    {
        std::vector<std::uint8_t> const rampT = marble::core::meshAssetV1BuildGardenTiltedRampTemplateBytes();
        if (!marble::core::meshAssetV1Valid(std::span(rampT))) {
            return 24;
        }
        std::vector<std::uint8_t> const benchT = marble::core::meshAssetV1BuildGardenBenchSlatsTemplateBytes();
        if (!marble::core::meshAssetV1Valid(std::span(benchT))) {
            return 25;
        }
        std::optional<marble::core::MeshAssetV1CpuViews> const bv = marble::core::meshAssetV1TryParse(std::span(benchT));
        if (!bv.has_value() || bv->vertexCount != 144u || bv->indexCount != 216u) {
            return 26;
        }
        std::vector<std::uint8_t> baked;
        marble::math::Vec3 const ctr{12.f, 3.f, -5.f};
        marble::math::Vec3 const half{1.f, 0.5f, 0.5f};
        if (!marble::core::meshAssetV1BakeGardenPropInstanceToWorldBytes(
                std::span<std::uint8_t const>(benchT.data(), benchT.size()),
                ctr,
                half,
                0.2f,
                -0.15f,
                0.05f,
                baked)) {
            return 27;
        }
        std::optional<marble::core::MeshAssetV1CpuViews> const bakedViews = marble::core::meshAssetV1TryParse(std::span(baked));
        if (!bakedViews.has_value()) {
            return 28;
        }
        marble::math::Vec3 p0{};
        if (!marble::core::meshAssetV1ReadPosition(*bakedViews, 0u, p0)) {
            return 29;
        }
        if (!std::isfinite(p0.x) || !std::isfinite(p0.y) || !std::isfinite(p0.z)) {
            return 30;
        }
    }

    return 0;
}
