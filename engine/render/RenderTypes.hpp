#pragma once

#include "math/Mat4.hpp"
#include "math/Vec3.hpp"
#include "render/MaterialId.hpp"

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
    /// Pipeline slot: `kMaterialDefault`, `kMaterialTranslucent`, etc. Unknown ids map to default in `VulkanRhi`.
    std::uint8_t materialId = kMaterialDefault;
    /// Logical draw bucket: lower layers are recorded first within a frame (before `FrameOverlayTint`).
    std::uint8_t drawLayer = 0;
    /// Reserved; keeps size a multiple of 16 for stable layout if mirrored to GPU later.
    std::uint8_t _padding[22]{};
};

static_assert(sizeof(MeshDrawInstance) % 16 == 0, "MeshDrawInstance size for GPU packing");
static_assert(kMaterialPipelineSlotCount >= 2, "MaterialId and MeshDrawInstance assume at least default+translucent");

} // namespace marble::render
