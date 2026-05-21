#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace marble::core {

namespace asset_pack_v1_write_detail {

inline void writeLeU32(std::ofstream& out, std::uint32_t v) {
    unsigned char const b[4] = {
        static_cast<unsigned char>(v & 0xffu),
        static_cast<unsigned char>((v >> 8u) & 0xffu),
        static_cast<unsigned char>((v >> 16u) & 0xffu),
        static_cast<unsigned char>((v >> 24u) & 0xffu),
    };
    out.write(reinterpret_cast<char const*>(b), 4);
}

inline void writeLeU64(std::ofstream& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        char const c = static_cast<char>((v >> (8 * i)) & 0xffu);
        out.write(&c, 1);
    }
}

} // namespace asset_pack_v1_write_detail

/// Write Marble asset pack v1: magic `MARBPK01`, LE version 1, TOC + raw blobs (matches [`ReadOnlyAssetPack`](ReadOnlyAssetPack.hpp)).
[[nodiscard]] inline bool writeAssetPackV1(
    std::filesystem::path const& packPath,
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> const& entries
) {
    using asset_pack_v1_write_detail::writeLeU32;
    using asset_pack_v1_write_detail::writeLeU64;

    static constexpr char kMagic[8] = {'M', 'A', 'R', 'B', 'P', 'K', '0', '1'};
    constexpr std::uint32_t kMaxEntries = 1'000'000u;

    if (entries.size() > kMaxEntries) {
        return false;
    }
    std::set<std::string> seen;
    std::uint64_t tocBytes = 16;
    for (auto const& e : entries) {
        std::string const& key = e.first;
        if (key.empty() || key.size() > 4096) {
            return false;
        }
        if (!seen.insert(key).second) {
            return false;
        }
        std::uint64_t const add = 4u + static_cast<std::uint64_t>(key.size()) + 8u + 8u;
        if (tocBytes > UINT64_MAX - add) {
            return false;
        }
        tocBytes += add;
    }

    std::uint64_t blobCursor = tocBytes;
    std::vector<std::tuple<std::string, std::uint64_t, std::uint64_t, std::vector<std::uint8_t> const*>> layout;
    layout.reserve(entries.size());
    for (auto const& e : entries) {
        std::uint64_t const sz = e.second.size();
        if (blobCursor > UINT64_MAX - sz) {
            return false;
        }
        layout.emplace_back(e.first, blobCursor, sz, &e.second);
        blobCursor += sz;
    }

    std::ofstream out(packPath, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(kMagic, 8);
    writeLeU32(out, 1u);
    writeLeU32(out, static_cast<std::uint32_t>(entries.size()));
    for (auto const& t : layout) {
        std::string const& key = std::get<0>(t);
        std::uint64_t const off = std::get<1>(t);
        std::uint64_t const sz = std::get<2>(t);
        writeLeU32(out, static_cast<std::uint32_t>(key.size()));
        out.write(key.data(), static_cast<std::streamsize>(key.size()));
        writeLeU64(out, off);
        writeLeU64(out, sz);
    }
    for (auto const& t : layout) {
        std::vector<std::uint8_t> const* blob = std::get<3>(t);
        if (!blob->empty()) {
            out.write(reinterpret_cast<char const*>(blob->data()), static_cast<std::streamsize>(blob->size()));
        }
    }
    return static_cast<bool>(out);
}

} // namespace marble::core
