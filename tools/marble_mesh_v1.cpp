#include "core/MeshAssetV1.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: marble_mesh_v1 <out.mesh> --demo-triangle|--demo-ramp\n";
        return 2;
    }
    std::filesystem::path const out = argv[1];
    char const* const mode = argv[2];
    bool okWrite = false;
    if (std::strcmp(mode, "--demo-triangle") == 0) {
        okWrite = marble::core::writeMeshAssetV1DemoTriangleFile(out);
    } else if (std::strcmp(mode, "--demo-ramp") == 0) {
        std::vector<std::uint8_t> const bytes = marble::core::meshAssetV1BuildDemoRampBytes();
        okWrite = marble::core::writeMeshAssetV1BytesToFile(out, std::span<std::uint8_t const>(bytes.data(), bytes.size()));
    } else {
        std::cerr << "usage: marble_mesh_v1 <out.mesh> --demo-triangle|--demo-ramp\n";
        return 2;
    }
    if (!okWrite) {
        std::cerr << "marble_mesh_v1: failed to write " << out << '\n';
        return 1;
    }
    std::vector<std::uint8_t> verify;
    if (!marble::platform::filesystem::readBinaryFile(out, verify)) {
        std::cerr << "marble_mesh_v1: failed to re-read " << out << '\n';
        return 1;
    }
    if (!marble::core::meshAssetV1Valid(std::span<std::uint8_t const>(verify.data(), verify.size()))) {
        std::cerr << "marble_mesh_v1: written file failed validation\n";
        return 1;
    }
    return 0;
}
