// Two identical fixed-step runs over a garden dynamic sphere + shared marble input step; positions must match
// bit-for-bit on one host (mirrors `garden_server` + `applyGardenMarblePlayerStep`).

#include "garden/GardenAuthorityTick.hpp"
#include "garden/GardenMarblePlayer.hpp"
#include "garden/GardenSimulation.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include "gameplay/SimulationIsland.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <span>

using namespace marble::physics;
using marble::math::Vec3;

namespace {

[[nodiscard]] Vec3 runOnce() {
    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(42u, layout);

    auto physicsScene = createJoltPhysicsScene();
    marble::gameplay::SimulationIsland island{};

    PhysicsStaticHeightFieldDesc hfDesc{};
    hfDesc.offset = marble::gameplay::localGameplayToJolt(island, layout.terrain.origin);
    hfDesc.scale = {layout.terrain.cellSize, 1.f, layout.terrain.cellSize};
    hfDesc.sampleCount = layout.terrain.sampleCount;
    hfDesc.heights = std::span<float const>(layout.terrain.heights.data(), layout.terrain.heights.size());
    hfDesc.material.restitution = 0.35f;
    hfDesc.material.friction = 0.7f;
    static_cast<void>(physicsScene->addStaticHeightField(hfDesc));

    std::array<RigidBodyKinematics, 2> pair{};
    marble::garden::placeMarblesInArena(pair, layout);
    RigidBodyKinematics marble = pair[0];

    PhysicsDynamicSphereDesc desc{};
    desc.center = marble::gameplay::localGameplayToJolt(island, marble.position);
    desc.linearVelocity = marble.linearVelocity;
    desc.radius = marble::garden::kMarbleRadius;
    desc.invMass = marble.invMass;
    desc.material.restitution = 0.672f;
    desc.material.friction = 0.42f;
    desc.material.linearDamping = 0.02f;
    desc.material.angularDamping = 0.10f;
    desc.enhancedInternalEdgeRemoval = true;
    PhysicsBodyId const body = physicsScene->addDynamicSphere(desc);
    assert(body != kInvalidPhysicsBodyId);
    physicsScene->optimizeBroadPhase();

    PhysicsWorldSettings const worldSettings = marble::garden::gardenAuthorityPhysicsWorldSettings();

    constexpr float kDt = 1.f / 60.f;
    bool jumpWasHeld = false;
    float jumpCharge = 0.f;

    PhysicsCylindricalXZClamp clamp{};
    clamp.maxHorizontalRadiusFromYAxis =
        marble::garden::kGardenRadius - marble::garden::kMarbleRadius - 0.02f;
    clamp.minCenterY = -5.f;
    PhysicsStepOptions stepOpts{};
    stepOpts.postStepCylindricalClamp = &clamp;
    std::array<PhysicsBodyId, 1> ids{{body}};
    stepOpts.clampBodyIds = std::span<PhysicsBodyId const>(ids.data(), 1u);

    for (int i = 0; i < 180; ++i) {
        marble::garden::applyGardenMarblePlayerStep(
            marble,
            layout,
            (i % 40 < 20) ? 0.35f : -0.2f,
            (i % 55 < 28) ? 0.6f : 0.f,
            false,
            jumpWasHeld,
            jumpCharge,
            kDt,
            marble::garden::kMarbleRadius);

        physicsScene->syncHostVelocitiesBeforeStep(ids, &marble, 1u);
        physicsScene->step(kDt, worldSettings, stepOpts);
        physicsScene->readBackKinematics(ids, &marble, 1u);
    }
    return marble.position;
}

} // namespace

int main() {
    Vec3 const a = runOnce();
    Vec3 const b = runOnce();
    if (a.x != b.x || a.y != b.y || a.z != b.z) {
        std::printf("determinism mismatch: (%.6f,%.6f,%.6f) vs (%.6f,%.6f,%.6f)\n",
            static_cast<double>(a.x), static_cast<double>(a.y), static_cast<double>(a.z),
            static_cast<double>(b.x), static_cast<double>(b.y), static_cast<double>(b.z));
        return 2;
    }
    std::printf("garden_marble_determinism_test: OK (%.3f, %.3f, %.3f)\n",
        static_cast<double>(a.x), static_cast<double>(a.y), static_cast<double>(a.z));
    return 0;
}
