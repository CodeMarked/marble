#include "core/AssetPackV1Write.hpp"
#include "core/ReadOnlyAssetPack.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
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
    stdfs::path const tmp = stdfs::temp_directory_path() / "marble_asset_pack_write_roundtrip_test";
    stdfs::create_directories(tmp, ec);
    stdfs::path const packFile = tmp / "roundtrip.marbpak";

    std::vector<std::uint8_t> const payload = minimalSpirv(7u);
    if (!marble::core::writeAssetPackV1(packFile, {{"shaders/test_roundtrip.spv", payload}})) {
        stdfs::remove_all(tmp, ec);
        return 1;
    }

    marble::core::ReadOnlyAssetPack pack;
    if (!pack.open(packFile)) {
        stdfs::remove_all(tmp, ec);
        return 2;
    }
    std::vector<std::uint8_t> readBack;
    if (!pack.tryRead("shaders/test_roundtrip.spv", readBack)) {
        stdfs::remove_all(tmp, ec);
        return 3;
    }
    if (readBack != payload) {
        stdfs::remove_all(tmp, ec);
        return 4;
    }

    stdfs::remove_all(tmp, ec);
    return 0;
}
