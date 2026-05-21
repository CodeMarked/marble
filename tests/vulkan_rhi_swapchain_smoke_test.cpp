// Opt-in smoke test (MARBLE_BUILD_VULKAN_SMOKE_TEST): real Vulkan swapchain, drawFrame, forced recreate.
// Requires MARBLE_SHADER_SPV_DIR (see tests/CMakeLists.txt) and a working WSI/display.

#include "math/Mat4.hpp"
#include "platform/window/Window.hpp"
#include "render/RenderTypes.hpp"
#include "render/vulkan/VulkanRhi.hpp"
#include "shared/LoadMeshSampleSpirv.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <span>
#include <vector>

int main() {
    char const* dir = std::getenv("MARBLE_SHADER_SPV_DIR");
    if (dir == nullptr || std::strlen(dir) == 0) {
        return 1;
    }

    std::vector<std::uint8_t> vertSpirv;
    std::vector<std::uint8_t> fragSpirv;
    std::vector<std::uint8_t> emissiveSpirv;
    if (!marble::game_shared::loadSampleMeshSpirvFromShaderDirectory(std::filesystem::path(dir), vertSpirv, fragSpirv,
                                                                     emissiveSpirv)) {
        return 2;
    }

    auto window = marble::platform::Window::create("vulkan_rhi_swapchain_smoke_test", 128, 96);
    if (!window) {
        return 3;
    }

    marble::render::VulkanRhi rhi;
    if (!rhi.initFromSpirvBytes(
            *window,
            "vulkan_rhi_swapchain_smoke_test",
            std::span<std::uint8_t const>(vertSpirv.data(), vertSpirv.size()),
            std::span<std::uint8_t const>(fragSpirv.data(), fragSpirv.size()),
            std::nullopt,
            std::span<std::uint8_t const>(emissiveSpirv.data(), emissiveSpirv.size()))) {
        return 4;
    }

    marble::math::Mat4 const viewProj = marble::math::Mat4::identity();
    std::span<marble::render::MeshDrawInstance const> const emptyDraws{};

    for (int i = 0; i < 4; ++i) {
        if (!rhi.drawFrame(*window, viewProj, emptyDraws)) {
            return 10 + i;
        }
        window->pollEvents();
    }

    window->markFramebufferResized();
    for (int i = 0; i < 8; ++i) {
        if (!rhi.drawFrame(*window, viewProj, emptyDraws)) {
            return 20 + i;
        }
        window->pollEvents();
    }

    rhi.shutdown();
    return 0;
}
