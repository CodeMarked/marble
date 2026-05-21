#include "MarbleSampleShaderNames.hpp"
#include "core/SpirvBytecode.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

namespace {

[[nodiscard]] bool validateSpirvFile(std::filesystem::path const& p) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(p, ec) || ec) {
        std::cerr << "sample_mesh_spirv_artifact_test: not a file: " << p << '\n';
        return false;
    }
    std::uintmax_t const sz = std::filesystem::file_size(p, ec);
    if (ec || sz == 0 || sz > marble::core::kMaxSpirvBytecodeBytes) {
        std::cerr << "sample_mesh_spirv_artifact_test: bad file size: " << p << '\n';
        return false;
    }
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    std::ifstream in(p, std::ios::binary);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!in || static_cast<std::size_t>(in.gcount()) != buf.size()) {
        std::cerr << "sample_mesh_spirv_artifact_test: read failed: " << p << '\n';
        return false;
    }
    if (!marble::core::spirvBytecodeHeaderValid(std::span<std::uint8_t const>(buf))) {
        std::cerr << "sample_mesh_spirv_artifact_test: SPIR-V header invalid: " << p << '\n';
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
