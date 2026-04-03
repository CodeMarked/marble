#pragma once

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

/// Minimal Vulkan swapchain + single graphics pipeline + mesh uploads for game use.
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

    using DrawCommand = MeshDrawInstance;

    VulkanRhi();
    VulkanRhi(VulkanRhi const&) = delete;
    VulkanRhi& operator=(VulkanRhi const&) = delete;
    VulkanRhi(VulkanRhi&&) noexcept;
    VulkanRhi& operator=(VulkanRhi&&) noexcept;
    ~VulkanRhi();

    /// Loads SPIR-V from `shaderDirectory` (`mesh.vert.spv`, `mesh.frag.spv`).
    /// `physicalDeviceIndex`, when set, selects the **n**th suitable adapter after sorting (discrete before integrated).
    [[nodiscard]] bool init(
        platform::Window& window,
        char const* appName,
        std::string shaderDirectory,
        std::optional<std::uint32_t> physicalDeviceIndex = std::nullopt
    );

    /// Same device/swapchain/pipeline setup as `init`, using in-memory SPIR-V (sizes must be multiples of 4).
    [[nodiscard]] bool initFromSpirvBytes(
        platform::Window& window,
        char const* appName,
        std::span<std::uint8_t const> vertSpirv,
        std::span<std::uint8_t const> fragSpirv,
        std::optional<std::uint32_t> physicalDeviceIndex = std::nullopt
    );

    void shutdown();

    /// Upload geometry; returns index for `MeshDrawInstance::meshIndex`, or `UINT32_MAX` on failure.
    [[nodiscard]] std::uint32_t uploadMesh(std::span<Vertex const> vertices, std::span<std::uint32_t const> indices);

    /// Replace an existing mesh slot (frees previous GPU buffers). Returns false if `meshIndex` is out of range.
    [[nodiscard]] bool replaceMesh(std::uint32_t meshIndex, std::span<Vertex const> vertices, std::span<std::uint32_t const> indices);

    void setClearColor(float r, float g, float b, float a) override;

    /// Records and presents one frame. Recreates the swapchain when the window is resized or `OUT_OF_DATE`.
    /// Returns false on unrecoverable failure.
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
