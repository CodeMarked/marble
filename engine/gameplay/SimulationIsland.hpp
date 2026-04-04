#pragma once

#include "math/Geometry.hpp"
#include "math/Vec3.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"

namespace marble::gameplay {

/// Double-precision anchor of the active simulation **island** in universe/world space.
/// Gameplay stores rigid bodies and authored layout in **simulation-local** [`Vec3`](../../math/Vec3.hpp)
/// (meters relative to this anchor). [`IPhysicsScene`](../../physics/IPhysicsScene.hpp) / Jolt use the same
/// local frame: **world** (for replication, UI, globe) = local + origin in double where needed.
///
/// See ADR-0059 (simulation space and physics tier contract).
struct SimulationIsland {
    double originX{};
    double originY{};
    double originZ{};
};

/// Universe/world position (double) for replication or debug; not used on the GPU.
struct WorldPosition3 {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] constexpr WorldPosition3 worldPositionFromLocal(
    SimulationIsland const& island,
    math::Vec3 local
) noexcept {
    return WorldPosition3{
        static_cast<double>(local.x) + island.originX,
        static_cast<double>(local.y) + island.originY,
        static_cast<double>(local.z) + island.originZ,
    };
}

/// Map gameplay **simulation-local** coordinates into Jolt/scene space. Uses double for
/// `local + origin - origin` so large anchors do not collapse small offsets when those
/// offsets are represented as `float` elsewhere in the pipeline.
[[nodiscard]] inline math::Vec3 localGameplayToJolt(
    SimulationIsland const& island,
    math::Vec3 localGameplay
) noexcept {
    double const wx = static_cast<double>(localGameplay.x) + island.originX;
    double const wy = static_cast<double>(localGameplay.y) + island.originY;
    double const wz = static_cast<double>(localGameplay.z) + island.originZ;
    return math::Vec3{
        static_cast<float>(wx - island.originX),
        static_cast<float>(wy - island.originY),
        static_cast<float>(wz - island.originZ),
    };
}

/// Map a position already expressed in **universe** space (float components, possibly large)
/// into Jolt/scene space by subtracting the island origin in double.
[[nodiscard]] inline math::Vec3 universePositionToJolt(
    SimulationIsland const& island,
    math::Vec3 universePosition
) noexcept {
    double const wx = static_cast<double>(universePosition.x);
    double const wy = static_cast<double>(universePosition.y);
    double const wz = static_cast<double>(universePosition.z);
    return math::Vec3{
        static_cast<float>(wx - island.originX),
        static_cast<float>(wy - island.originY),
        static_cast<float>(wz - island.originZ),
    };
}

[[nodiscard]] inline math::Aabb localGameplayAabbToJolt(
    SimulationIsland const& island,
    math::Aabb const& localAabb
) noexcept {
    return math::Aabb{
        localGameplayToJolt(island, localAabb.min),
        localGameplayToJolt(island, localAabb.max),
    };
}

[[nodiscard]] inline marble::physics::PhysicsStaticHeightFieldDesc localGameplayHeightFieldToJolt(
    SimulationIsland const& island,
    marble::physics::PhysicsStaticHeightFieldDesc const& localDesc
) noexcept {
    marble::physics::PhysicsStaticHeightFieldDesc out = localDesc;
    out.offset = localGameplayToJolt(island, localDesc.offset);
    return out;
}

} // namespace marble::gameplay
