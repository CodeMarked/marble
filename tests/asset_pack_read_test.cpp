#include "core/AssetPackV1Write.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "core/ResourceManager.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::vector<std::uint8_t> minimalSpirv(std::uint32_t boundWord) {
    std::vector<std::uint8_t> b(20);
    b[0] = 0x03;
    b[1] = 0x02;
    b[2] = 0x23;
    b[3] = 0x07;
    b[4] = 0x00;
    b[5] = 0x00;
    b[6] = 0x01;
    b[7] = 0x00;
    b[8] = 0x00;
    b[9] = 0x00;
    b[10] = 0x00;
    b[11] = 0x00;
    b[12] = static_cast<std::uint8_t>(boundWord & 0xffu);
    b[13] = static_cast<std::uint8_t>((boundWord >> 8u) & 0xffu);
    b[14] = static_cast<std::uint8_t>((boundWord >> 16u) & 0xffu);
    b[15] = static_cast<std::uint8_t>((boundWord >> 24u) & 0xffu);
    b[16] = 0x00;
    b[17] = 0x00;
    b[18] = 0x00;
    b[19] = 0x00;
    return b;
}

} // namespace

int main() {
    namespace stdfs = std::filesystem;
    std::error_code ec;

    const stdfs::path tmp = stdfs::temp_directory_path() / "marble_asset_pack_read_test";
    stdfs::create_directories(tmp, ec);

    {
        const stdfs::path packFile = tmp / "sample.marbpak";
        std::vector<std::uint8_t> const spv = minimalSpirv(1u);
        if (!marble::core::writeAssetPackV1(packFile, {{"shaders/from_pack.spv", spv}})) {
            stdfs::remove_all(tmp, ec);
            return 1;
        }
        marble::core::BinaryResourceManager<32> rm;
        rm.setReadOnlyPack(packFile);
        if (!rm.acquire("shaders/from_pack.spv", marble::core::ResourceKind::ShaderBytecode)) {
            stdfs::remove_all(tmp, ec);
            return 2;
        }
        marble::core::BinaryResource const* r = rm.find("shaders/from_pack.spv");
        if (r == nullptr || r->bytes.size() != 20 || r->category != marble::core::ResourceKind::ShaderBytecode ||
            r->bytes[12] != 1u) {
            stdfs::remove_all(tmp, ec);
            return 3;
        }
        if (r->resolvedPath != packFile) {
            stdfs::remove_all(tmp, ec);
            return 4;
        }
        if (!rm.release("shaders/from_pack.spv")) {
            stdfs::remove_all(tmp, ec);
            return 5;
        }
    }

    {
        const stdfs::path looseRoot = tmp / "loose";
        stdfs::create_directories(looseRoot / "shaders", ec);
        const stdfs::path packFile = tmp / "overlap.marbpak";
        std::vector<std::uint8_t> const packSpv = minimalSpirv(1u);
        std::vector<std::uint8_t> const looseSpv = minimalSpirv(42u);
        {
            std::ofstream looseOut(looseRoot / "shaders" / "overlap.spv", std::ios::binary);
            looseOut.write(reinterpret_cast<char const*>(looseSpv.data()), static_cast<std::streamsize>(looseSpv.size()));
        }
        if (!marble::core::writeAssetPackV1(packFile, {{"shaders/overlap.spv", packSpv}})) {
            stdfs::remove_all(tmp, ec);
            return 6;
        }
        marble::core::BinaryResourceManager<32> rm;
        rm.setSearchRoots({looseRoot});
        rm.setReadOnlyPack(packFile);
        if (!rm.acquire("shaders/overlap.spv", marble::core::ResourceKind::ShaderBytecode)) {
            stdfs::remove_all(tmp, ec);
            return 7;
        }
        marble::core::BinaryResource const* r = rm.find("shaders/overlap.spv");
        if (r == nullptr || r->bytes[12] != 42u) {
            stdfs::remove_all(tmp, ec);
            return 8;
        }
        if (r->resolvedPath != looseRoot / "shaders" / "overlap.spv") {
            stdfs::remove_all(tmp, ec);
            return 9;
        }
    }

    {
        const stdfs::path packFile = tmp / "mesh_only.marbpak";
        std::vector<std::uint8_t> const meshBytes = marble::core::meshAssetV1BuildDemoTriangleBytes();
        if (!marble::core::writeAssetPackV1(packFile, {{"meshes/from_pack.mesh", meshBytes}})) {
            stdfs::remove_all(tmp, ec);
            return 10;
        }
        marble::core::BinaryResourceManager<32> rm;
        rm.setReadOnlyPack(packFile);
        if (!rm.acquire("meshes/from_pack.mesh", marble::core::ResourceKind::Mesh)) {
            stdfs::remove_all(tmp, ec);
            return 11;
        }
        marble::core::BinaryResource const* r = rm.find("meshes/from_pack.mesh");
        if (r == nullptr || r->category != marble::core::ResourceKind::Mesh || r->resolvedPath != packFile) {
            stdfs::remove_all(tmp, ec);
            return 12;
        }
        std::optional<marble::core::MeshAssetV1CpuViews> const views = marble::core::meshAssetV1ViewsFrom(r);
        if (!views.has_value() || views->vertexCount != 3u || views->indexCount != 3u ||
            marble::core::meshAssetV1IndexAt(*views, 2).value_or(99u) != 2u) {
            stdfs::remove_all(tmp, ec);
            return 13;
        }
        if (!rm.release("meshes/from_pack.mesh")) {
            stdfs::remove_all(tmp, ec);
            return 14;
        }
    }

    stdfs::remove_all(tmp, ec);
    return 0;
}
