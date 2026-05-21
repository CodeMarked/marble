#pragma once

#include "core/ClosedHashTable.hpp"
#include "core/MeshAssetV1.hpp"
#include "core/ReadOnlyAssetPack.hpp"
#include "core/SpirvBytecode.hpp"
#include "core/StringId.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace marble::core {

/// Runtime categories for higher-level loaders (meshes, images, bytecode) built on `BinaryResourceManager`.
enum class ResourceKind : std::uint8_t {
    BinaryBlob,
    Image,
    Mesh,
    ShaderBytecode,
};

struct BinaryResource {
    StringId guid{};
    std::string virtualPath{};
    std::filesystem::path resolvedPath{};
    std::vector<std::uint8_t> bytes{};
    std::size_t refCount{0};
    ResourceKind category{ResourceKind::BinaryBlob};
};

[[nodiscard]] inline bool virtualPathMatchesResourceKind(std::string_view virtualPath, ResourceKind kind) {
    if (virtualPath.empty()) {
        return false;
    }
    switch (kind) {
    case ResourceKind::BinaryBlob:
        return true;
    case ResourceKind::ShaderBytecode: {
        constexpr std::string_view suffix = ".spv";
        if (virtualPath.size() < suffix.size()) {
            return false;
        }
        return virtualPath.substr(virtualPath.size() - suffix.size()) == suffix;
    }
    case ResourceKind::Image: {
        static constexpr std::string_view kImageSuffixes[] = {".png", ".jpg", ".jpeg", ".ktx", ".ktx2"};
        for (std::string_view const suf : kImageSuffixes) {
            if (virtualPath.size() >= suf.size() && virtualPath.substr(virtualPath.size() - suf.size()) == suf) {
                return true;
            }
        }
        return false;
    }
    case ResourceKind::Mesh: {
        constexpr std::string_view suffix = ".mesh";
        if (virtualPath.size() < suffix.size()) {
            return false;
        }
        return virtualPath.substr(virtualPath.size() - suffix.size()) == suffix;
    }
    }
    return false;
}

/// Non-owning SPIR-V bytecode view for a loaded `BinaryResource` (category + `spirvBytecodeHeaderValid`).
[[nodiscard]] inline std::optional<std::span<std::uint8_t const>> shaderBytecodeSpanFrom(BinaryResource const* r) noexcept {
    if (r == nullptr || r->category != ResourceKind::ShaderBytecode) {
        return std::nullopt;
    }
    std::span<std::uint8_t const> const span{r->bytes.data(), r->bytes.size()};
    if (!spirvBytecodeHeaderValid(span)) {
        return std::nullopt;
    }
    return span;
}

/// Validated non-owning views for `ResourceKind::Mesh` (`meshAssetV1TryParse` on stored bytes).
[[nodiscard]] inline std::optional<MeshAssetV1CpuViews> meshAssetV1ViewsFrom(BinaryResource const* r) noexcept {
    if (r == nullptr || r->category != ResourceKind::Mesh) {
        return std::nullopt;
    }
    return meshAssetV1TryParse(std::span<std::uint8_t const>(r->bytes.data(), r->bytes.size()));
}

/// Runtime resource registry and loader baseline (book §7.2.2.1, §7.2.2.5, §7.2.2.6).
/// - Keeps at most one copy of each virtual resource path in memory.
/// - Supports ref-counted acquire/release lifetime management.
/// - Uses synchronous file reads only (async/streaming is deferred).
///
/// **v0 resolution order for `acquire`:** try `searchRoots_` (loose files via `findOnSearchPath` + `readBinaryFile`)
/// first; if no loose file matches the virtual path, try the optional attached read-only pack (`ReadOnlyAssetPack`).
/// Virtual path strings are the same for both sources. `BinaryResource::resolvedPath` is the loose file path when
/// loaded from disk, or the pack file path when loaded from the pack.
template <std::size_t Capacity = 512>
class BinaryResourceManager {
public:
    void setSearchRoots(std::vector<std::filesystem::path> roots) { searchRoots_ = std::move(roots); }

    /// Attach a read-only pack searched after loose `searchRoots_` miss. Empty path clears the pack.
    void setReadOnlyPack(std::filesystem::path const& packPath) {
        readOnlyPack_.reset();
        if (packPath.empty()) {
            return;
        }
        auto p = std::make_unique<ReadOnlyAssetPack>();
        if (!p->open(packPath)) {
            return;
        }
        readOnlyPack_ = std::move(p);
    }

    void clearReadOnlyPack() noexcept { readOnlyPack_.reset(); }

    [[nodiscard]] bool acquire(std::string_view virtualPath) { return acquire(virtualPath, ResourceKind::BinaryBlob); }

    [[nodiscard]] bool acquire(std::string_view virtualPath, ResourceKind kind) {
        if (virtualPath.empty()) {
            return false;
        }
        if (!virtualPathMatchesResourceKind(virtualPath, kind)) {
            return false;
        }
        const StringId id = makeStringId(virtualPath);
        if (BinaryResource* existing = resources_.find(id)) {
            if (existing->virtualPath != virtualPath || existing->category != kind) {
                return false;
            }
            ++existing->refCount;
            return true;
        }

        std::optional<std::filesystem::path> resolved = platform::filesystem::findOnSearchPath(
            std::filesystem::path(virtualPath), searchRoots_);
        std::vector<std::uint8_t> bytes;
        if (resolved.has_value()) {
            if (!platform::filesystem::readBinaryFile(*resolved, bytes)) {
                return false;
            }
        } else if (readOnlyPack_ && readOnlyPack_->tryRead(virtualPath, bytes)) {
            resolved = readOnlyPack_->packPath();
        } else {
            return false;
        }

        if (kind == ResourceKind::ShaderBytecode && !spirvBytecodeHeaderValid(bytes)) {
            return false;
        }
        if (kind == ResourceKind::Mesh && !meshAssetV1Valid(std::span<std::uint8_t const>(bytes.data(), bytes.size()))) {
            return false;
        }

        BinaryResource resource{};
        resource.guid = id;
        resource.virtualPath = std::string(virtualPath);
        resource.resolvedPath = *resolved;
        resource.bytes = std::move(bytes);
        resource.refCount = 1;
        resource.category = kind;
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
    std::unique_ptr<ReadOnlyAssetPack> readOnlyPack_{};
};

/// Point the manager at a single runtime root (for example `Engine::assetsRootPath()`).
template <std::size_t Capacity>
[[nodiscard]] inline bool setBinaryResourceSearchRoot(
    BinaryResourceManager<Capacity>& mgr,
    std::filesystem::path const& absoluteRoot
) {
    if (absoluteRoot.empty()) {
        return false;
    }
    mgr.setSearchRoots({absoluteRoot});
    return true;
}

} // namespace marble::core
