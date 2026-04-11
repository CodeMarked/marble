#include "MarbleSampleShaderNames.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

constexpr std::uint32_t kSpirvMagic = 0x07230203;

[[nodiscard]] bool validateSpirvFile(std::filesystem::path const& p) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(p, ec) || ec) {
        std::cerr << "sample_mesh_spirv_artifact_test: not a file: " << p << '\n';
        return false;
    }
    std::uintmax_t const sz = std::filesystem::file_size(p, ec);
    if (ec || sz == 0 || sz % 4 != 0) {
        std::cerr << "sample_mesh_spirv_artifact_test: bad size (empty or not multiple of 4): " << p << '\n';
        return false;
    }
    std::ifstream in(p, std::ios::binary);
    std::uint32_t magic = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (!in || magic != kSpirvMagic) {
        std::cerr << "sample_mesh_spirv_artifact_test: missing SPIR-V magic word: " << p << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    char const* dir = std::getenv("MARBLE_SHADER_SPV_DIR");
    if (dir == nullptr || std::strlen(dir) == 0) {
        std::cerr << "sample_mesh_spirv_artifact_test: MARBLE_SHADER_SPV_DIR is not set\n";
        return 1;
    }
    std::filesystem::path const root(dir);
    for (std::size_t i = 0; i < marble::game_shared::kMarbleSampleShaderCount; ++i) {
        if (!validateSpirvFile(root / marble::game_shared::kMarbleSampleShaderSpvBasenames[i])) {
            return 1;
        }
    }
    return 0;
}
