#include "core/AssetPackV1Write.hpp"

#include "platform/filesystem/FileSystem.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 4 || (argc - 2) % 2 != 0) {
        std::cerr << "usage: marble_pack_v1 <out.marbpak> <virtualPath> <diskPath> [<virtualPath> <diskPath> ...]\n";
        return 2;
    }

    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> entries;
    entries.reserve(static_cast<std::size_t>((argc - 2) / 2));

    for (int i = 2; i < argc; i += 2) {
        std::string const vpath = argv[i];
        std::filesystem::path const disk = argv[i + 1];
        std::vector<std::uint8_t> bytes;
        if (!marble::platform::filesystem::readBinaryFile(disk, bytes)) {
            std::cerr << "marble_pack_v1: failed to read " << disk << '\n';
            return 1;
        }
        entries.emplace_back(vpath, std::move(bytes));
    }

    if (!marble::core::writeAssetPackV1(std::filesystem::path(argv[1]), entries)) {
        std::cerr << "marble_pack_v1: failed to write " << argv[1] << '\n';
        return 1;
    }
    return 0;
}
