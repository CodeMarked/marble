#pragma once

#include "math/Geometry.hpp"
#include "math/Vec3.hpp"
#include "physics/CollisionMiddleware.hpp"

#include <cstdint>
#include <span>

namespace marble::physics {

/// Opaque handle for a body in a middleware [`IPhysicsScene`](IPhysicsScene.hpp). `0` is invalid.
using PhysicsBodyId = std::uint32_t;

inline constexpr PhysicsBodyId kInvalidPhysicsBodyId = 0;

/// Contact and motion damping for static or dynamic bodies (middleware backends map to their material model).
struct PhysicsBodyMaterial {
    float restitution = 0.3f;
    float friction = 0.55f;
    float linearDamping = 0.02f;
    float angularDamping = 0.1f;
};

/// Axis-aligned box at rest in world space (center derived from `bounds`).
struct PhysicsStaticBoxDesc {
    math::Aabb bounds{};
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Static sphere at `center` with `radius` (meters). Gameplay/simulation-local space.
struct PhysicsStaticSphereDesc {
    math::Vec3 center{};
    float radius = 0.1f;
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Static oriented box: half extents along **body** axes, same yaw/pitch/roll convention as
/// [`marble::math::Mat4::rotationY`] * rotationX * rotationZ on column vectors (Garden props).
struct PhysicsStaticOrientedBoxDesc {
    math::Vec3 center{};
    math::Vec3 halfExtents{};
    float yawRadians{};
    float pitchRadians{};
    float rollRadians{};
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Static capsule: Jolt intrinsic spine on **+Y** from `-halfHeight` to `+halfHeight` at body origin.
/// `intrinsicCylinderAxis` in **body** space before yaw/pitch/roll: 0 = spine along +Y, 1 = along +X, 2 = along +Z.
struct PhysicsStaticCapsuleDesc {
    math::Vec3 center{};
    float halfHeight = 0.25f;
    float radius = 0.05f;
    float yawRadians{};
    float pitchRadians{};
    float rollRadians{};
    std::uint8_t intrinsicCylinderAxis = 0; ///< 0 = +Y, 1 = +X, 2 = +Z in body space before YPR.
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Static convex hull: points in **body-local** space (before yaw/pitch/roll), same composition order as
/// [`PhysicsStaticOrientedBoxDesc`]. World pose is `center` + YPR on the rigid body. Implementations copy `points`
/// synchronously; the span need only be valid for the duration of [`IPhysicsScene::addStaticConvexHull`].
struct PhysicsStaticConvexHullDesc {
    math::Vec3 center{};
    float yawRadians{};
    float pitchRadians{};
    float rollRadians{};
    std::span<math::Vec3 const> points{};
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Static single-sided triangle mesh (indexed). Vertices and indices are **body-local**; CCW winding when viewed from
/// the collision (front) side per Jolt. `indices` length must be a multiple of 3. Copied synchronously by the scene.
struct PhysicsStaticTriangleMeshDesc {
    math::Vec3 center{};
    float yawRadians{};
    float pitchRadians{};
    float rollRadians{};
    std::span<math::Vec3 const> vertices{};
    std::span<std::uint32_t const> indices{};
    bool enhancedInternalEdgeRemoval = true;
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Static heightfield: Jolt surface `offset + scale * (ix, height[ix,iz], iz)` with `ix,iz ∈ [0, sampleCount-1]`.
/// `heights` is row-major `iz * sampleCount + ix`, length `sampleCount²`.
struct PhysicsStaticHeightFieldDesc {
    math::Vec3 offset{};
    math::Vec3 scale{1.f, 1.f, 1.f};
    std::uint32_t sampleCount{};
    std::span<float const> heights{};
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Dynamic sphere with game-authored pose and mass.
struct PhysicsDynamicSphereDesc {
    math::Vec3 center{};
    math::Vec3 linearVelocity{};
    float radius = 0.11f;
    float invMass = 1.f;
    /// When true, Jolt uses extra work for contacts against mesh internal edges (recommended for spheres on [`MeshShape`]).
    bool enhancedInternalEdgeRemoval = false;
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Dynamic capsule (cylinder with hemispherical caps) aligned along +Y.
/// Total height = `2 * halfHeight + 2 * radius`.
struct PhysicsDynamicCapsuleDesc {
    math::Vec3 center{};
    math::Vec3 linearVelocity{};
    float halfHeight = 0.5f;
    float radius = 0.25f;
    float invMass = 1.f;
    PhysicsBodyMaterial material{};
    CollisionFilter filter{0xFFFFFFFFu, 0xFFFFFFFFu};
};

/// Optional horizontal cylinder about +Y through the origin: clamps body **center** after the solver (gameplay bounds).
struct PhysicsCylindricalXZClamp {
    /// Max allowed horizontal distance `sqrt(x*x + z*z)` for the body center (meters).
    float maxHorizontalRadiusFromYAxis = 0.f;
    /// Minimum world `y` for the body center (meters).
    float minCenterY = 0.f;
};

/// Per-step options (post-step clamps, future: debug draw hooks). Stage B may add query flags.
struct PhysicsStepOptions {
    /// When non-null and `clampBodyIds` is non-empty, listed bodies are clamped in Jolt after `Update`.
    PhysicsCylindricalXZClamp const* postStepCylindricalClamp = nullptr;
    std::span<PhysicsBodyId const> clampBodyIds{};
};

} // namespace marble::physics
