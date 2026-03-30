#pragma once

#include "core/ClosedHashTable.hpp"
#include "core/StringId.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace marble::core {

struct BinaryResource {
    StringId guid{};
    std::string virtualPath{};
    std::filesystem::path resolvedPath{};
    std::vector<std::uint8_t> bytes{};
    std::size_t refCount{0};
};

/// Runtime resource registry and loader baseline (book §7.2.2.1, §7.2.2.5, §7.2.2.6).
/// - Keeps at most one copy of each virtual resource path in memory.
/// - Supports ref-counted acquire/release lifetime management.
/// - Uses synchronous file reads only (async/streaming is deferred).
template <std::size_t Capacity = 512>
class BinaryResourceManager {
public:
    void setSearchRoots(std::vector<std::filesystem::path> roots) { searchRoots_ = std::move(roots); }

    [[nodiscard]] bool acquire(std::string_view virtualPath) {
        if (virtualPath.empty()) {
            return false;
        }
        const StringId id = makeStringId(virtualPath);
        if (BinaryResource* existing = resources_.find(id)) {
            if (existing->virtualPath == virtualPath) {
                ++existing->refCount;
                return true;
            }
            return false;
        }

        const auto resolved = platform::filesystem::findOnSearchPath(std::filesystem::path(virtualPath), searchRoots_);
        if (!resolved.has_value()) {
            return false;
        }

        BinaryResource resource{};
        resource.guid = id;
        resource.virtualPath = std::string(virtualPath);
        resource.resolvedPath = *resolved;
        if (!platform::filesystem::readBinaryFile(resource.resolvedPath, resource.bytes)) {
            return false;
        }
        resource.refCount = 1;
        return resources_.insertOrAssign(id, std::move(resource));
    }

    [[nodiscard]] bool release(std::string_view virtualPath) {
        const StringId id = makeStringId(virtualPath);
        BinaryResource* r = resources_.find(id);
        if (r == nullptr || r->virtualPath != virtualPath || r->refCount == 0) {
            return false;
        }
        --r->refCount;
        if (r->refCount == 0) {
            const bool erased = resources_.erase(id);
            (void)erased;
        }
        return true;
    }

    [[nodiscard]] BinaryResource const* find(std::string_view virtualPath) const {
        const StringId id = makeStringId(virtualPath);
        const BinaryResource* r = resources_.find(id);
        if (r == nullptr || r->virtualPath != virtualPath) {
            return nullptr;
        }
        return r;
    }

    [[nodiscard]] std::optional<std::size_t> refCountOf(std::string_view virtualPath) const {
        const BinaryResource* r = find(virtualPath);
        if (r == nullptr) {
            return std::nullopt;
        }
        return r->refCount;
    }

    [[nodiscard]] std::size_t loadedCount() const noexcept { return resources_.size(); }

private:
    ClosedHashTable<StringId, BinaryResource, Capacity, StringIdHash> resources_{};
    std::vector<std::filesystem::path> searchRoots_{};
};

} // namespace marble::core
