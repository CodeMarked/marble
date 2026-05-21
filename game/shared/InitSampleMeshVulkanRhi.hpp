#pragma once

#include "shared/LoadMeshSampleSpirv.hpp"
#include "shared/SampleMeshShaderResources.hpp"

#include "core/ResourceManager.hpp"
#include "platform/window/Window.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace marble::game_shared {

/// Registry + pack (when `assetsRoot` non-empty), else SPIR-V files under `shaderDirectory`.
/// Does not set clear color or window title. Returns false only when disk fallback also fails.
template <std::size_t Capacity>
[[nodiscard]] inline bool initSampleMeshVulkanRhiFromAssetsOrShaderDirectory(
    marble::platform::Window& window,
    marble::render::VulkanRhi& rhi,
    marble::core::BinaryResourceManager<Capacity>& assetRegistry,
    std::string const& assetsRoot,
    std::filesystem::path const& shaderDirectory,
    char const* appName,
    std::optional<std::uint32_t> physicalDeviceIndex = std::nullopt
) {
    bool vkOk = false;
    if (!assetsRoot.empty()) {
        (void)marble::core::setBinaryResourceSearchRoot(assetRegistry, std::filesystem::path(assetsRoot));
        (void)tryAttachSampleMeshShadersPack(assetRegistry, assetsRoot);
        if (acquireAllSampleMeshRegistryShaders(assetRegistry)) {
            std::span<std::uint8_t const> vspan;
            std::span<std::uint8_t const> fspan;
            std::span<std::uint8_t const> espan;
            if (sampleMeshRegistrySpirvSpansForInit(assetRegistry, vspan, fspan, espan)) {
                vkOk = rhi.initFromSpirvBytes(window, appName, vspan, fspan, physicalDeviceIndex, espan);
            }
        }
    }
    if (!vkOk) {
        std::vector<std::uint8_t> vertDisk;
        std::vector<std::uint8_t> fragDisk;
        std::vector<std::uint8_t> emDisk;
        if (!loadSampleMeshSpirvFromShaderDirectory(shaderDirectory, vertDisk, fragDisk, emDisk) ||
            !rhi.initFromSpirvBytes(
                window,
                appName,
                std::span(vertDisk.data(), vertDisk.size()),
                std::span(fragDisk.data(), fragDisk.size()),
                physicalDeviceIndex,
                std::span(emDisk.data(), emDisk.size()))) {
            return false;
        }
    }
    return true;
}

} // namespace marble::game_shared
