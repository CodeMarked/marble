#pragma once

#include "garden/GardenSimulation.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <algorithm>
#include <cmath>

namespace marble::garden {

/// Camera-relative wish on world XZ (`wishX`, `wishZ` may be non-unit; normalized internally) plus charged jump.
/// Matches offline [`GardenGame`](GardenGame.cpp) roll + jump and dedicated `garden_server` authority.
inline void applyGardenMarblePlayerStep(
    physics::RigidBodyKinematics& kin,
    GardenLayout const& layout,
    float wishX,
    float wishZ,
    bool jumpHeld,
    bool& jumpWasHeld,
    float& jumpChargeSec,
    float dt,
    float marbleRadius) noexcept {
    using marble::physics::applyImpulseLinear;
    if (dt <= 0.f || kin.invMass <= 0.f) {
        return;
    }

    float const mag = std::sqrt(wishX * wishX + wishZ * wishZ);
    if (mag > 1e-5f) {
        float const nx = wishX / mag;
        float const nz = wishZ / mag;
        math::Vec3 const wish{nx, 0.f, nz};
        applyImpulseLinear(kin, wish * (kGardenMarbleRollStrength * dt));
        float const vx = kin.linearVelocity.x;
        float const vz = kin.linearVelocity.z;
        float const vh = std::sqrt(vx * vx + vz * vz);
        if (vh > kGardenMarbleMaxHorizSpeed && vh > 1e-6f) {
            float const s = kGardenMarbleMaxHorizSpeed / vh;
            kin.linearVelocity.x *= s;
            kin.linearVelocity.z *= s;
        }
    }

    if (jumpHeld) {
        jumpChargeSec += dt;
        jumpChargeSec = std::min(jumpChargeSec, kGardenJumpChargeMaxSec);
    } else {
        if (jumpWasHeld && jumpChargeSec > 1e-4f && gardenBallOnGround(layout, kin, marbleRadius)) {
            float const imp = gardenJumpImpulseFromHoldSeconds(jumpChargeSec);
            applyImpulseLinear(kin, math::Vec3{0.f, imp, 0.f});
        }
        jumpChargeSec = 0.f;
    }
    jumpWasHeld = jumpHeld;
}

} // namespace marble::garden
