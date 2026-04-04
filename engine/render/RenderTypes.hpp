#pragma once

#include "math/Mat4.hpp"
#include "math/Vec3.hpp"

#include <cstdint>

namespace marble::render {

/// Optional fullscreen tint drawn after scene geometry (e.g. pause dimming). `a <= 0` skips the pass.
struct FrameOverlayTint {
    float r{};
    float g{};
    float b{};
    float a{};
};

/// GPU-agnostic mesh instance submitted by gameplay or a scene pass.
/// Concrete RHIs map this to bound vertex buffers, push constants, or indirect draws.
struct MeshDrawInstance {
    std::uint32_t meshIndex = 0;
    math::Mat4 model = math::Mat4::identity();
    math::Vec3 color{1.f, 1.f, 1.f};
    /// Multiplied with RGB in the fragment shader alpha channel (`pc.albedo.a`).
    float colorAlpha = 1.f;
    /// Push constant flags: see `DrawFlags.hpp` (`kPcFlagClipSpace`, `kPcFlagUnlit`, etc.).
    std::uint32_t drawFlags = 0;
};

} // namespace marble::render
