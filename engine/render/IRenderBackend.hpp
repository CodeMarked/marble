#pragma once

#include "math/Mat4.hpp"
#include "render/RenderTypes.hpp"

#include <span>

namespace marble::platform {
class Window;
}

namespace marble::render {

/// Narrow seam between frame loop / `IRenderPhase` and a concrete graphics backend.
/// Keeps draw submission types in `render/` while implementations stay backend-specific.
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    /// When `overlayTint` is non-null and `overlayTint->a > 0`, draws a blended fullscreen tint after `draws`.
    [[nodiscard]] virtual bool drawFrame(
        platform::Window& window,
        math::Mat4 const& viewProj,
        std::span<MeshDrawInstance const> draws,
        FrameOverlayTint const* overlayTint = nullptr
    ) = 0;

    [[nodiscard]] virtual bool initialized() const = 0;

    /// Swapchain / surface clear color for subsequent frames. Default: no-op (stubs, headless).
    virtual void setClearColor(float /*r*/, float /*g*/, float /*b*/, float /*a*/) {}
};

} // namespace marble::render
