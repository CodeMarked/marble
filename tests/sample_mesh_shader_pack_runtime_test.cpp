#include "MarbleSampleShaderNames.hpp"
#include "core/AssetPackV1Write.hpp"
#include "core/SpirvBytecode.hpp"
#include "platform/filesystem/FileSystem.hpp"
#include "shared/SampleMeshShaderResources.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

int main() {
    char const* dir = std::getenv("MARBLE_SHADER_SPV_DIR");
    if (dir == nullptr || std::strlen(dir) == 0) {
        std::cerr << "sample_mesh_shader_pack_runtime_test: MARBLE_SHADER_SPV_DIR is not set\n";
        return 1;
    }
    namespace stdfs = std::filesystem;
    stdfs::path const spvDir(dir);
    std::error_code ec;
    stdfs::path const tmp = stdfs::temp_directory_path() / "marble_sample_mesh_shader_pack_runtime_test";
    stdfs::remove_all(tmp, ec);
    stdfs::create_directories(tmp, ec);

    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> entries;
    entries.reserve(marble::game_shared::kMarbleSampleShaderCount);
    for (std::size_t i = 0; i < marble::game_shared::kMarbleSampleShaderCount; ++i) {
        std::vector<std::uint8_t> bytes;
        if (!marble::platform::filesystem::readBinaryFile(
                spvDir / marble::game_shared::kMarbleSampleShaderSpvBasenames[i], bytes)) {
            std::cerr << "sample_mesh_shader_pack_runtime_test: failed to read SPIR-V artifact\n";
            stdfs::remove_all(tmp, ec);
            return 2;
        }
        entries.emplace_back(std::string(marble::game_shared::kMarbleSampleShaderRegistryPaths[i]), std::move(bytes));
    }

    std::string const assetsRoot = tmp.string();
    stdfs::path const packPath = tmp / std::string(marble::game_shared::kMarbleSampleMeshShadersPackFileName);
    if (!marble::core::writeAssetPackV1(packPath, entries)) {
        std::cerr << "sample_mesh_shader_pack_runtime_test: writeAssetPackV1 failed\n";
        stdfs::remove_all(tmp, ec);
        return 3;
    }

    marble::core::BinaryResourceManager<32> rm;
    rm.setSearchRoots({stdfs::path(assetsRoot)});
    if (!marble::game_shared::tryAttachSampleMeshShadersPack(rm, assetsRoot)) {
        std::cerr << "sample_mesh_shader_pack_runtime_test: tryAttachSampleMeshShadersPack failed\n";
        stdfs::remove_all(tmp, ec);
        return 4;
    }
    if (!marble::game_shared::acquireAllSampleMeshRegistryShaders(rm)) {
        std::cerr << "sample_mesh_shader_pack_runtime_test: acquireAllSampleMeshRegistryShaders failed\n";
        stdfs::remove_all(tmp, ec);
        return 5;
    }
    std::span<std::uint8_t const> vspan;
    std::span<std::uint8_t const> fspan;
    std::span<std::uint8_t const> espan;
    if (!marble::game_shared::sampleMeshRegistrySpirvSpansForInit(rm, vspan, fspan, espan)) {
        std::cerr << "sample_mesh_shader_pack_runtime_test: sampleMeshRegistrySpirvSpansForInit failed\n";
        stdfs::remove_all(tmp, ec);
        return 6;
    }
    if (!marble::core::spirvBytecodeHeaderValid(vspan) || !marble::core::spirvBytecodeHeaderValid(fspan) ||
        !marble::core::spirvBytecodeHeaderValid(espan)) {
        std::cerr << "sample_mesh_shader_pack_runtime_test: SPIR-V header invalid\n";
        stdfs::remove_all(tmp, ec);
        return 7;
    }

    stdfs::remove_all(tmp, ec);
    return 0;
}
