#pragma once

#include "render/vulkan/VulkanRhi.hpp"

#include <cstdint>
#include <vector>

namespace marble::ui {

/// Append a screen-axis-aligned rectangle (pixel coords, origin top-left) as clip-space triangles.
inline void appendNdcUnlitScreenRectPx(
    float leftPx,
    float topPx,
    float rightPx,
    float bottomPx,
    int framebufferWidth,
    int framebufferHeight,
    float cr,
    float cg,
    float cb,
    std::vector<marble::render::VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx
) {
    if (framebufferWidth <= 0 || framebufferHeight <= 0 || rightPx <= leftPx || bottomPx <= topPx) {
        return;
    }
    float const invHw = 2.f / static_cast<float>(framebufferWidth);
    float const invHh = 2.f / static_cast<float>(framebufferHeight);
    auto toNdc = [&](float px, float py) {
        float const nx = px * invHw - 1.f;
        float const ny = 1.f - py * invHh;
        return std::pair<float, float>{nx, ny};
    };
    auto const tl = toNdc(leftPx, topPx);
    auto const tr = toNdc(rightPx, topPx);
    auto const br = toNdc(rightPx, bottomPx);
    auto const bl = toNdc(leftPx, bottomPx);

    std::uint32_t const base = static_cast<std::uint32_t>(vtx.size());
    auto push = [&](float nx, float ny) {
        marble::render::VulkanRhi::Vertex v{};
        v.px = nx;
        v.py = ny;
        v.pz = 0.f;
        v.nx = 0.f;
        v.ny = 0.f;
        v.nz = 1.f;
        v.cr = cr;
        v.cg = cg;
        v.cb = cb;
        vtx.push_back(v);
    };
    push(tl.first, tl.second);
    push(tr.first, tr.second);
    push(br.first, br.second);
    push(bl.first, bl.second);
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base);
}

} // namespace marble::ui
