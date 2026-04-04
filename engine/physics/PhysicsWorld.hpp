#pragma once

#include "math/Vec3.hpp"

#include <cstddef>
#include <cstdint>

namespace marble::physics {

struct RigidBodyKinematics;

/// Tunables for a **game-owned** physics step. [`SimplePhysicsWorld`](PhysicsIntegration.hpp) applies only
/// `gravity` and `maxSubSteps` today. Fields marked **middleware** are ignored by `SimplePhysicsWorld` but consumed
/// by [`IPhysicsScene`](IPhysicsScene.hpp) implementations (see [`ADR-0058`](../../docs/decisions/ADR-0058-physics-middleware-integration.md)).
struct PhysicsWorldSettings {
    math::Vec3 gravity{0.f, -9.81f, 0.f};
    bool enableContinuousCollision{};
    std::uint8_t maxSubSteps{1u};
    /// Middleware: `0` = backend default (Jolt uses its internal defaults).
    std::uint8_t velocitySolverIterations{};
    /// Middleware: `0` = backend default.
    std::uint8_t positionSolverIterations{};
    /// Middleware: when true, backends may enable sleeping; `SimplePhysicsWorld` ignores.
    bool enableSleeping{};
};

/// Abstract world step used by samples; [`SimplePhysicsWorld`](PhysicsIntegration.hpp) is the default
/// implementation. A future Jolt (or other) adapter implements the same interface for Stage A/B migration
/// ([ADR-0058](../../docs/decisions/ADR-0058-physics-middleware-integration.md)).
class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() noexcept = default;

    virtual void setSettings(PhysicsWorldSettings settings) noexcept = 0;
    [[nodiscard]] virtual PhysicsWorldSettings settings() const noexcept = 0;
    virtual void step(float deltaSeconds, RigidBodyKinematics* bodies, std::size_t bodyCount) noexcept = 0;
};

} // namespace marble::physics
