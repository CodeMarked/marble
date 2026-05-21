#include "core/MeshAssetV1.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "render/MeshAssetV1HostDecode.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

int main() {
    std::vector<std::uint8_t> const bytes = marble::core::meshAssetV1BuildDemoTriangleBytes();
    std::optional<marble::core::MeshAssetV1CpuViews> const views = marble::core::meshAssetV1TryParse(std::span(bytes));
    if (!views.has_value()) {
        return 1;
    }
    std::vector<marble::render::VulkanRhi::Vertex> verts;
    std::vector<std::uint32_t> idx;
    if (!marble::render::meshAssetV1CpuViewsToHostBuffers(*views, verts, idx)) {
        return 2;
    }
    if (verts.size() != 3 || idx.size() != 3) {
        return 3;
    }
    if (idx[0] != 0u || idx[1] != 1u || idx[2] != 2u) {
        return 4;
    }
    marble::render::VulkanRhi::Vertex const& v0 = verts[0];
    if (v0.px != 0.f || v0.py != 0.f || v0.pz != 0.f || v0.nx != 0.f || v0.ny != 0.f || v0.nz != 1.f || v0.cr != 1.f ||
        v0.cg != 1.f || v0.cb != 1.f) {
        return 5;
    }
    marble::core::MeshAssetV1CpuViews const empty{};
    std::vector<marble::render::VulkanRhi::Vertex> v2;
    std::vector<std::uint32_t> i2;
    if (marble::render::meshAssetV1CpuViewsToHostBuffers(empty, v2, i2)) {
        return 6;
    }
    return 0;
}
