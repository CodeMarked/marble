#pragma once

#include "platform/filesystem/Path.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace marble::platform::filesystem {

[[nodiscard]] inline bool exists(std::filesystem::path const& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec);
}

[[nodiscard]] inline bool readBinaryFile(std::filesystem::path const& path, std::vector<std::uint8_t>& outBytes) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff len = in.tellg();
    if (len < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    outBytes.resize(static_cast<std::size_t>(len));
    if (len == 0) {
        return true;
    }
    in.read(reinterpret_cast<char*>(outBytes.data()), len);
    return static_cast<std::streamoff>(in.gcount()) == len;
}

[[nodiscard]] inline bool readTextFile(std::filesystem::path const& path, std::string& outText) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff len = in.tellg();
    if (len < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    outText.resize(static_cast<std::size_t>(len));
    if (len == 0) {
        return true;
    }
    in.read(outText.data(), len);
    return static_cast<std::streamoff>(in.gcount()) == len;
}

[[nodiscard]] inline std::optional<std::filesystem::path> findOnSearchPath(
    std::filesystem::path const& relativeFile,
    std::vector<std::filesystem::path> const& roots) {
    if (relativeFile.is_absolute()) {
        if (marble::platform::filesystem::exists(relativeFile)) {
            return normalize(relativeFile);
        }
        return std::nullopt;
    }
    for (auto const& root : roots) {
        const std::filesystem::path candidate = normalize(root / relativeFile);
        if (marble::platform::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace marble::platform::filesystem
