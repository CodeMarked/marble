#pragma once

#include "render/vulkan/VulkanRhi.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace marble::ui {

/// Fills `outVtx` / `outIdx` with clip-space mesh data for `text` (ASCII), using stb_easy_font quads.
/// Vertex positions are NDC xy for use with `kPcFlagClipSpace` (and typically `kPcFlagUnlit`).
void appendEasyFontTextPx(
    std::string const& text,
    float originPxX,
    float originPxY,
    int framebufferWidth,
    int framebufferHeight,
    float cr,
    float cg,
    float cb,
    std::vector<marble::render::VulkanRhi::Vertex>& outVtx,
    std::vector<std::uint32_t>& outIdx
);

} // namespace marble::ui
