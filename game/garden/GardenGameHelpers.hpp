#pragma once

#include "garden/GardenSimulation.hpp"
#include "math/Geometry.hpp"
#include "math/Vec3.hpp"
#include "physics/RigidBodyDynamics.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include <cstdint>
#include <vector>

namespace marble::garden_app::detail {

void fillTerrainMeshCpu(
    marble::garden::GardenTerrain const& t,
    std::vector<marble::render::VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx
);

[[nodiscard]] marble::math::Vec3 colorForWallBrick(marble::math::Aabb const& b) noexcept;
[[nodiscard]] marble::math::Vec3 colorForKind(marble::garden::GardenColliderKind k, std::uint32_t salt) noexcept;

void worldRayFromWindowPixel(
    float mouseX,
    float mouseY,
    int fbW,
    int fbH,
    marble::math::Vec3 eye,
    marble::math::Vec3 target,
    float fovyRad,
    float aspect,
    marble::math::Vec3& outOrigin,
    marble::math::Vec3& outDir
) noexcept;

void applyLawnStrokeImpulse(
    marble::physics::RigidBodyKinematics& marble,
    marble::math::Vec3 const& pStrokeStart,
    marble::math::Vec3 const& pStrokeLast,
    marble::math::Vec3 const& pStrokePrev,
    float flickVelEmaX,
    float flickVelEmaZ,
    bool flickEmaValid,
    float deltaSeconds
) noexcept;

} // namespace marble::garden_app::detail
