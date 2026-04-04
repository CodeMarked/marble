#pragma once

#include "math/Vec3.hpp"

namespace marble::physics {

/// Translational state for a rigid body with **inverse mass** (`0` means kinematic / infinite mass).
struct RigidBodyKinematics {
    math::Vec3 position{};
    math::Vec3 linearVelocity{};
    float invMass{};
};

/// Semi-implicit (symplectic) Euler: `v += a * dt`, then `x += v * dt` (book Ch.13 integration baseline).
inline void integrateSemiImplicitEuler(
    RigidBodyKinematics& body,
    math::Vec3 accelerationWorld,
    float deltaSeconds
) noexcept {
    if (body.invMass <= 0.f || deltaSeconds <= 0.f) {
        return;
    }
    body.linearVelocity = body.linearVelocity + accelerationWorld * deltaSeconds;
    body.position = body.position + body.linearVelocity * deltaSeconds;
}

/// `deltaV = J * invMass` for a world-space impulse `J`.
inline void applyImpulseLinear(RigidBodyKinematics& body, math::Vec3 impulseWorld) noexcept {
    if (body.invMass <= 0.f) {
        return;
    }
    body.linearVelocity = body.linearVelocity + impulseWorld * body.invMass;
}

/// Scalar yaw spin about **+Y** (matches engine left-handed +Y-up convention; ADR-0017).
/// `invInertiaY == 0` locks rotation.
struct RigidBodyYaw {
    float yawRadians{};
    float angularVelocityY{};
    float invInertiaY{};
};

/// `α = τ * invI`, then `ω += α dt`, `θ += ω dt`.
inline void integrateYawAxis(
    RigidBodyYaw& body,
    float torqueAboutY,
    float deltaSeconds
) noexcept {
    if (body.invInertiaY <= 0.f || deltaSeconds <= 0.f) {
        return;
    }
    const float alpha = torqueAboutY * body.invInertiaY;
    body.angularVelocityY += alpha * deltaSeconds;
    body.yawRadians += body.angularVelocityY * deltaSeconds;
}

} // namespace marble::physics
