#pragma once

#include "math/Mat4.hpp"
#include "math/Vec3.hpp"

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
class VulkanRhi {
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

    struct DrawCommand {
        std::uint32_t meshIndex = 0;
        math::Mat4 model = math::Mat4::identity();
        math::Vec3 color{1.f, 1.f, 1.f};
    };

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

    void shutdown();

    /// Upload geometry; returns index for `DrawCommand::meshIndex`, or `UINT32_MAX` on failure.
    [[nodiscard]] std::uint32_t uploadMesh(std::span<Vertex const> vertices, std::span<std::uint32_t const> indices);

    void setClearColor(float r, float g, float b, float a);

    /// Records and presents one frame. Recreates the swapchain when the window is resized or `OUT_OF_DATE`.
    /// Returns false on unrecoverable failure.
    [[nodiscard]] bool drawFrame(platform::Window& window, math::Mat4 const& viewProj, std::span<DrawCommand const> draws);

    [[nodiscard]] bool initialized() const;

private:
    std::unique_ptr<VulkanRhiImpl> impl_;
};

} // namespace marble::render
