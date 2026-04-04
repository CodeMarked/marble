#pragma once

#include "math/Mat4.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace marble::physics {

/// Game-agnostic rigid-body **scene** backed by middleware (Jolt). No game layouts or sample constants.
///
/// Extension points (Stage B): additional `add*` overloads (capsule, convex hull), collision filters mapped from
/// [`CollisionFilter`](CollisionMiddleware.hpp), sleeping/CCD driven from [`PhysicsWorldSettings`](PhysicsWorld.hpp).
class IPhysicsScene {
public:
    virtual ~IPhysicsScene() noexcept = default;

    virtual void clear() = 0;

    [[nodiscard]] virtual PhysicsBodyId addStaticBox(PhysicsStaticBoxDesc const& desc) = 0;
    [[nodiscard]] virtual PhysicsBodyId addStaticHeightField(PhysicsStaticHeightFieldDesc const& desc) = 0;
    [[nodiscard]] virtual PhysicsBodyId addDynamicSphere(PhysicsDynamicSphereDesc const& desc) = 0;

    virtual void removeBody(PhysicsBodyId id) = 0;

    /// Call after a batch of adds (or `clear` + rebuild) so broadphase structures match the new set.
    virtual void optimizeBroadPhase() = 0;

    /// Push host linear velocities into the solver before [`step`]. `ids.size()` must equal `count`.
    virtual void syncHostVelocitiesBeforeStep(
        std::span<PhysicsBodyId const> ids,
        RigidBodyKinematics const* hostKinematics,
        std::size_t count
    ) = 0;

    virtual void step(
        float deltaSeconds,
        PhysicsWorldSettings const& settings,
        PhysicsStepOptions const& options = {}
    ) = 0;

    /// Read world-space position (COM) and linear velocity into `out`. `ids.size()` must equal `count`.
    virtual void readBackKinematics(std::span<PhysicsBodyId const> ids, RigidBodyKinematics* out, std::size_t count)
        const = 0;

    /// Teleport / respawn: set world-space COM and linear velocity. Invalid `id` is a no-op.
    virtual void setBodyCenterAndLinearVelocity(PhysicsBodyId id, math::Vec3 center, math::Vec3 linearVelocity) = 0;

    /// Wake a dynamic body (e.g. after changing global gravity so sleeping bodies integrate again). Invalid `id` is a no-op.
    virtual void activateBody(PhysicsBodyId id) = 0;

    /// COM pose for rendering: column-major translation + rotation (scale applied by caller). Invalid `id` → identity.
    [[nodiscard]] virtual math::Mat4 bodyWorldMatrix(PhysicsBodyId id) const = 0;
};

/// Builds a Jolt-backed scene ([`ADR-0058`](../../docs/decisions/ADR-0058-physics-middleware-integration.md)).
[[nodiscard]] std::unique_ptr<IPhysicsScene> createJoltPhysicsScene();

} // namespace marble::physics
