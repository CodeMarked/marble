#include "math/Mat4.hpp"
#include "platform/window/Window.hpp"
#include "render/IRenderBackend.hpp"
#include "render/RenderTypes.hpp"

#include <span>

namespace {

struct StubBackend final : marble::render::IRenderBackend {
    bool drawFrame(
        marble::platform::Window&,
        marble::math::Mat4 const&,
        std::span<marble::render::MeshDrawInstance const>,
        marble::render::FrameOverlayTint const* = nullptr
    ) override {
        ++drawCalls;
        return true;
    }

    [[nodiscard]] bool initialized() const override { return true; }

    void setClearColor(float r, float g, float b, float a) override {
        ++clearColorCalls;
        lastR = r;
        lastG = g;
        lastB = b;
        lastA = a;
    }

    int drawCalls = 0;
    int clearColorCalls = 0;
    float lastR = 0.f;
    float lastG = 0.f;
    float lastB = 0.f;
    float lastA = 0.f;
};

} // namespace

int main() {
    auto window = marble::platform::Window::create("irender_backend_test", 64, 64);
    if (!window) {
        return 0;
    }

    StubBackend backend{};
    if (!backend.initialized()) {
        return 1;
    }

    marble::math::Mat4 const viewProj = marble::math::Mat4::identity();
    std::span<marble::render::MeshDrawInstance const> const empty{};

    if (!backend.drawFrame(*window, viewProj, empty)) {
        return 2;
    }
    if (backend.drawCalls != 1) {
        return 3;
    }

    backend.setClearColor(0.1f, 0.2f, 0.3f, 0.4f);
    if (backend.clearColorCalls != 1 || backend.lastR != 0.1f || backend.lastG != 0.2f || backend.lastB != 0.3f ||
        backend.lastA != 0.4f) {
        return 4;
    }

    return 0;
}
