#include "core/ResourceManager.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>

int main() {
    namespace stdfs = std::filesystem;

    const stdfs::path root = stdfs::temp_directory_path() / "marble_resource_manager_test";
    std::error_code ec;
    stdfs::create_directories(root / "pkg1", ec);
    stdfs::create_directories(root / "pkg2", ec);

    {
        std::ofstream out(root / "pkg1" / "a.bin", std::ios::binary);
        const std::array<std::uint8_t, 3> bytes{1u, 2u, 3u};
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    {
        std::ofstream out(root / "pkg2" / "b.bin", std::ios::binary);
        const std::array<std::uint8_t, 2> bytes{7u, 8u};
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    marble::core::BinaryResourceManager<32> rm;
    rm.setSearchRoots({root / "pkg2", root / "pkg1"});

    if (!rm.acquire("a.bin")) {
        stdfs::remove_all(root, ec);
        return 1;
    }
    if (!rm.acquire("a.bin")) {
        stdfs::remove_all(root, ec);
        return 2;
    }
    const auto rcA = rm.refCountOf("a.bin");
    if (!rcA.has_value() || *rcA != 2 || rm.loadedCount() != 1) {
        stdfs::remove_all(root, ec);
        return 3;
    }

    const auto* a = rm.find("a.bin");
    if (a == nullptr || a->bytes.size() != 3 || a->bytes[0] != 1u || a->bytes[2] != 3u) {
        stdfs::remove_all(root, ec);
        return 4;
    }

    if (!rm.acquire("b.bin") || rm.loadedCount() != 2) {
        stdfs::remove_all(root, ec);
        return 5;
    }
    const auto* b = rm.find("b.bin");
    if (b == nullptr || b->bytes.size() != 2 || b->bytes[0] != 7u) {
        stdfs::remove_all(root, ec);
        return 6;
    }

    if (!rm.release("a.bin")) {
        stdfs::remove_all(root, ec);
        return 7;
    }
    if (rm.loadedCount() != 2 || rm.refCountOf("a.bin").value_or(0) != 1) {
        stdfs::remove_all(root, ec);
        return 8;
    }
    if (!rm.release("a.bin")) {
        stdfs::remove_all(root, ec);
        return 9;
    }
    if (rm.find("a.bin") != nullptr || rm.loadedCount() != 1) {
        stdfs::remove_all(root, ec);
        return 10;
    }

    if (rm.acquire("missing.bin")) {
        stdfs::remove_all(root, ec);
        return 11;
    }

    stdfs::remove_all(root, ec);

    {
        const stdfs::path assetsRoot = stdfs::temp_directory_path() / "marble_rm_shader_assets";
        stdfs::create_directories(assetsRoot / "shaders", ec);
        {
            std::ofstream out(assetsRoot / "shaders" / "dummy.spv", std::ios::binary);
            const std::array<std::uint8_t, 4> spv{0x03, 0x02, 0x23, 0x07};
            out.write(reinterpret_cast<const char*>(spv.data()), static_cast<std::streamsize>(spv.size()));
        }
        marble::core::BinaryResourceManager<32> rmAssets;
        rmAssets.setSearchRoots({assetsRoot});
        if (!rmAssets.acquire("shaders/dummy.spv")) {
            stdfs::remove_all(assetsRoot, ec);
            return 12;
        }
        const auto* dummy = rmAssets.find("shaders/dummy.spv");
        if (dummy == nullptr || dummy->bytes.size() != 4) {
            stdfs::remove_all(assetsRoot, ec);
            return 13;
        }
        stdfs::remove_all(assetsRoot, ec);
    }

    return 0;
}
