#include "platform/filesystem/FileSystem.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main() {
    namespace fs = marble::platform::filesystem;
    namespace stdfs = std::filesystem;

    const stdfs::path root = stdfs::temp_directory_path() / "marble_fs_test";
    std::error_code ec;
    stdfs::create_directories(root / "a", ec);
    stdfs::create_directories(root / "b", ec);

    const stdfs::path textFile = root / "a" / "hello.txt";
    const stdfs::path binFile = root / "b" / "blob.bin";

    {
        std::ofstream out(textFile, std::ios::binary);
        out << "hello filesystem";
    }
    {
        std::ofstream out(binFile, std::ios::binary);
        const std::array<std::uint8_t, 4> bytes{1u, 2u, 3u, 255u};
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::string text;
    if (!fs::readTextFile(textFile, text) || text != "hello filesystem") {
        stdfs::remove_all(root, ec);
        return 1;
    }

    std::vector<std::uint8_t> bytes;
    if (!fs::readBinaryFile(binFile, bytes) || bytes.size() != 4 || bytes[0] != 1u || bytes[3] != 255u) {
        stdfs::remove_all(root, ec);
        return 2;
    }

    const auto found = fs::findOnSearchPath("hello.txt", {root / "b", root / "a"});
    if (!found.has_value() || found->filename().string() != "hello.txt") {
        stdfs::remove_all(root, ec);
        return 3;
    }

    stdfs::remove_all(root, ec);
    return 0;
}
