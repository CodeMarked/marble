#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace marble::core {

[[nodiscard]] inline std::optional<std::uint32_t> readPackLeU32(std::ifstream& in) {
    unsigned char b[4];
    in.read(reinterpret_cast<char*>(b), 4);
    if (in.gcount() != 4) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8u) |
           (static_cast<std::uint32_t>(b[2]) << 16u) | (static_cast<std::uint32_t>(b[3]) << 24u);
}

[[nodiscard]] inline std::optional<std::uint64_t> readPackLeU64(std::ifstream& in) {
    unsigned char b[8];
    in.read(reinterpret_cast<char*>(b), 8);
    if (in.gcount() != 8) {
        return std::nullopt;
    }
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(b[i]) << (8 * i);
    }
    return v;
}

/// Read-only Marble asset pack v1: single file, TOC + raw blobs, little-endian, no compression.
/// Virtual paths use POSIX separators and must match `BinaryResourceManager::acquire` keys exactly.
class ReadOnlyAssetPack {
public:
    ReadOnlyAssetPack() = default;
    ReadOnlyAssetPack(ReadOnlyAssetPack const&) = delete;
    ReadOnlyAssetPack& operator=(ReadOnlyAssetPack const&) = delete;
    ReadOnlyAssetPack(ReadOnlyAssetPack&&) noexcept = default;
    ReadOnlyAssetPack& operator=(ReadOnlyAssetPack&&) noexcept = default;

    /// Parse pack at `packPath`. On failure, leaves the object empty (`close()` state).
    [[nodiscard]] bool open(std::filesystem::path packPath) {
        close();
        if (packPath.empty()) {
            return false;
        }
        std::error_code ec;
        if (!std::filesystem::is_regular_file(packPath, ec) || ec) {
            return false;
        }
        std::uintmax_t const fsz = std::filesystem::file_size(packPath, ec);
        if (ec || fsz < 16) {
            return false;
        }
        fileSize_ = static_cast<std::uint64_t>(fsz);
        std::ifstream in(packPath, std::ios::binary);
        if (!in) {
            fileSize_ = 0;
            return false;
        }
        char magic[8];
        in.read(magic, 8);
        if (in.gcount() != 8) {
            fileSize_ = 0;
            return false;
        }
        static constexpr char kExpected[8] = {'M', 'A', 'R', 'B', 'P', 'K', '0', '1'};
        if (std::memcmp(magic, kExpected, 8) != 0) {
            fileSize_ = 0;
            return false;
        }
        std::optional<std::uint32_t> const ver = readPackLeU32(in);
        std::optional<std::uint32_t> const nEnt = readPackLeU32(in);
        if (!ver.has_value() || !nEnt.has_value() || *ver != 1u || *nEnt > 1'000'000u) {
            fileSize_ = 0;
            return false;
        }
        std::uint32_t const n = *nEnt;
        index_.clear();
        index_.reserve(static_cast<std::size_t>(n));
        for (std::uint32_t i = 0; i < n; ++i) {
            std::optional<std::uint32_t> const plen = readPackLeU32(in);
            if (!plen.has_value() || *plen == 0u || *plen > 4096u) {
                index_.clear();
                fileSize_ = 0;
                return false;
            }
            std::vector<char> buf(static_cast<std::size_t>(*plen));
            in.read(buf.data(), static_cast<std::streamsize>(*plen));
            if (static_cast<std::uint32_t>(in.gcount()) != *plen) {
                index_.clear();
                fileSize_ = 0;
                return false;
            }
            std::string key(buf.data(), buf.size());
            std::optional<std::uint64_t> const off = readPackLeU64(in);
            std::optional<std::uint64_t> const sz = readPackLeU64(in);
            if (!off.has_value() || !sz.has_value()) {
                index_.clear();
                fileSize_ = 0;
                return false;
            }
            if (*sz > fileSize_ || *off > fileSize_ - *sz) {
                index_.clear();
                fileSize_ = 0;
                return false;
            }
            if (!index_.emplace(std::move(key), std::make_pair(*off, *sz)).second) {
                index_.clear();
                fileSize_ = 0;
                return false;
            }
        }
        packPath_ = std::move(packPath);
        return true;
    }

    void close() noexcept {
        index_.clear();
        packPath_.clear();
        fileSize_ = 0;
    }

    [[nodiscard]] bool isOpen() const noexcept { return !packPath_.empty(); }

    [[nodiscard]] std::filesystem::path const& packPath() const noexcept { return packPath_; }

    [[nodiscard]] bool tryRead(std::string_view virtualPath, std::vector<std::uint8_t>& out) const {
        out.clear();
        if (!isOpen()) {
            return false;
        }
        std::string const key(virtualPath);
        auto const it = index_.find(key);
        if (it == index_.end()) {
            return false;
        }
        std::uint64_t const off = it->second.first;
        std::uint64_t const sz = it->second.second;
        std::ifstream in(packPath_, std::ios::binary);
        if (!in) {
            return false;
        }
        in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
        if (!in) {
            return false;
        }
        out.resize(static_cast<std::size_t>(sz));
        if (sz == 0) {
            return true;
        }
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz));
        return static_cast<std::uint64_t>(in.gcount()) == sz;
    }

private:
    std::filesystem::path packPath_{};
    std::uint64_t fileSize_{0};
    std::unordered_map<std::string, std::pair<std::uint64_t, std::uint64_t>> index_{};
};

} // namespace marble::core
