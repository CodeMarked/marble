#include "ui/EasyFontMesh.hpp"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "third_party/stb/stb_easy_font.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <cstring>

namespace marble::ui {

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
) {
    if (framebufferWidth <= 0 || framebufferHeight <= 0 || text.empty()) {
        return;
    }

    std::vector<char> mut(text.begin(), text.end());
    mut.push_back('\0');

    constexpr int kBufSize = 99999;
    std::vector<char> buffer(static_cast<std::size_t>(kBufSize));
    int const numQuads = stb_easy_font_print(
        originPxX,
        originPxY,
        mut.data(),
        nullptr,
        buffer.data(),
        kBufSize
    );
    if (numQuads <= 0) {
        return;
    }

    float const invHw = 2.f / static_cast<float>(framebufferWidth);
    float const invHh = 2.f / static_cast<float>(framebufferHeight);

    std::size_t const base = outVtx.size();
    int const maxQuads = std::min(numQuads, kBufSize / 64);
    for (int q = 0; q < maxQuads; ++q) {
        for (int v = 0; v < 4; ++v) {
            char const* p = buffer.data() + (q * 4 + v) * 16;
            float x = 0.f;
            float y = 0.f;
            std::memcpy(&x, p, sizeof(float));
            std::memcpy(&y, p + sizeof(float), sizeof(float));
            marble::render::VulkanRhi::Vertex vertex{};
            vertex.px = x * invHw - 1.f;
            vertex.py = 1.f - y * invHh;
            vertex.pz = 0.f;
            vertex.nx = 0.f;
            vertex.ny = 0.f;
            vertex.nz = 1.f;
            vertex.cr = cr;
            vertex.cg = cg;
            vertex.cb = cb;
            outVtx.push_back(vertex);
        }
        std::uint32_t const b = static_cast<std::uint32_t>(base + static_cast<std::size_t>(q) * 4u);
        outIdx.push_back(b);
        outIdx.push_back(b + 1);
        outIdx.push_back(b + 2);
        outIdx.push_back(b + 2);
        outIdx.push_back(b + 3);
        outIdx.push_back(b);
    }
}

} // namespace marble::ui
