#pragma once

#include "core/MeshAssetV1.hpp"
#include "math/Mat4.hpp"
#include "math/Vec3.hpp"
#include "render/IRenderBackend.hpp"
#include "render/RenderTypes.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace marble::platform {
class Window;
}

namespace marble::render {

struct VulkanRhiImpl;

/// Minimal Vulkan swapchain + mesh material pipelines + mesh uploads for game use.
/// `MeshDrawInstance::materialId` / `drawLayer` select pipeline variant and sort order within the swapchain pass.
/// Shaders: use `initFromSpirvBytes` with SPIR-V from files, packs, or registry.
class VulkanRhi : public IRenderBackend {
public:
    struct Vertex {
        float px{};
        float py{};
        float pz{};
        float nx{};
        float ny{};
        float nz{};
        float cr{};
        float cg{};
        float cb{};
    };
    static_assert(sizeof(Vertex) == marble::core::kMeshAssetV1VertexStrideBytes);

    using DrawCommand = MeshDrawInstance;

    VulkanRhi();
    VulkanRhi(VulkanRhi const&) = delete;
    VulkanRhi& operator=(VulkanRhi const&) = delete;
    VulkanRhi(VulkanRhi&&) noexcept;
    VulkanRhi& operator=(VulkanRhi&&) noexcept;
    ~VulkanRhi();

    /// SPIR-V sizes must be multiples of 4. One vertex module; lit fragment plus optional emissive fragment
    /// (`kPcFlagMeshEmissive` on `MeshDrawInstance::drawFlags` when emissive SPIR-V was provided).
    /// `physicalDeviceIndex`, when set, selects the **n**th **suitable** adapter (swapchain + queues + extensions),
    /// after sorting by GPU class (discrete before integrated)—not the raw `vkEnumeratePhysicalDevices` index.
    [[nodiscard]] bool initFromSpirvBytes(
        platform::Window& window,
        char const* appName,
        std::span<std::uint8_t const> vertSpirv,
        std::span<std::uint8_t const> fragSpirv,
        std::optional<std::uint32_t> physicalDeviceIndex = std::nullopt,
        std::span<std::uint8_t const> fragEmissiveSpirv = {}
    );

    void shutdown();

    /// Upload geometry; returns index for `MeshDrawInstance::meshIndex`, or `UINT32_MAX` on failure.
    [[nodiscard]] std::uint32_t uploadMesh(std::span<Vertex const> vertices, std::span<std::uint32_t const> indices);

    /// Upload from validated MRBMESH1 CPU views (`meshAssetV1ViewsFrom` / `meshAssetV1TryParse` only). Empty
    /// `vertexCount` or `indexCount` returns `UINT32_MAX` (same contract as `uploadMesh` for non-empty geometry).
    [[nodiscard]] std::uint32_t uploadMeshFromMeshAssetV1CpuViews(marble::core::MeshAssetV1CpuViews const& views);

    /// Replace an existing mesh slot (frees previous GPU buffers). Returns false if `meshIndex` is out of range.
    [[nodiscard]] bool replaceMesh(std::uint32_t meshIndex, std::span<Vertex const> vertices, std::span<std::uint32_t const> indices);

    void setClearColor(float r, float g, float b, float a) override;

    /// Records and presents one frame. Recreates the swapchain when the window is resized or `OUT_OF_DATE`.
    /// Returns false on unrecoverable failure (including `VK_ERROR_DEVICE_LOST` / surface lost, or swapchain
    /// recreate aborted e.g. 0x0 framebuffer while minimized). Full recovery: `shutdown()` then
    /// `initFromSpirvBytes(...)` again.
    [[nodiscard]] bool drawFrame(
        platform::Window& window,
        math::Mat4 const& viewProj,
        std::span<MeshDrawInstance const> draws,
        FrameOverlayTint const* overlayTint = nullptr
    ) override;

    [[nodiscard]] bool initialized() const override;

private:
    void ensureFullscreenQuadMesh();

    std::unique_ptr<VulkanRhiImpl> impl_;
};

} // namespace marble::render
