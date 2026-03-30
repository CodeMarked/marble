#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace marble::platform::filesystem {

[[nodiscard]] inline std::filesystem::path normalize(std::filesystem::path p) {
    p = p.lexically_normal();
    return p.make_preferred();
}

[[nodiscard]] inline bool isAbsolute(std::filesystem::path const& p) {
    return p.is_absolute();
}

[[nodiscard]] inline std::filesystem::path join(std::filesystem::path const& base, std::filesystem::path const& leaf) {
    return normalize(base / leaf);
}

[[nodiscard]] inline std::filesystem::path directoryOf(std::filesystem::path const& p) {
    return p.parent_path();
}

[[nodiscard]] inline std::string filenameOf(std::filesystem::path const& p) {
    return p.filename().string();
}

[[nodiscard]] inline std::string extensionOf(std::filesystem::path const& p) {
    return p.extension().string();
}

[[nodiscard]] inline std::vector<std::filesystem::path> splitSearchPath(std::string_view searchPath) {
    std::vector<std::filesystem::path> out;
#if defined(_WIN32)
    constexpr char kDelim = ';';
#else
    constexpr char kDelim = ':';
#endif
    std::size_t start = 0;
    while (start <= searchPath.size()) {
        std::size_t end = searchPath.find(kDelim, start);
        if (end == std::string_view::npos) {
            end = searchPath.size();
        }
        const std::string_view token = searchPath.substr(start, end - start);
        if (!token.empty()) {
            out.emplace_back(token);
        }
        start = end + 1;
    }
    return out;
}

} // namespace marble::platform::filesystem
