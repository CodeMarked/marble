// Dedicated garden authority with zero connected peers must not advance Jolt (early exit after session.tick).

#include "garden/GardenAuthorityTick.hpp"
#include "garden/GardenSimulation.hpp"
#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "gameplay/UdpGameTransport.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <array>
#include <cstdint>
#include <span>

int main() {
    using namespace marble::gameplay;

    UdpGameTransport server{};
    if (!server.bind(0u)) {
        return 1;
    }

    AuthoritativeSession<> session{};
    SessionConfig cfg{};
    cfg.mode = MultiplayerMode::DedicatedServer;
    cfg.maxPlayers = 2u;
    cfg.simulationHz = 60u;
    cfg.snapshotHz = 20u;
    if (!session.initialize(cfg, &server)) {
        return 2;
    }

    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(11u, layout);

    auto physics = marble::physics::createJoltPhysicsScene();
    SimulationIsland island{};
    marble::garden::gardenAuthorityPopulateStaticCollidersFromLayout(*physics, layout, island);

    std::array<marble::physics::RigidBodyKinematics, 2> marbles{};
    marble::garden::fillGardenServerMarbleSpawnStates(std::span(marbles.data(), marbles.size()), layout);
    marbles[0].position.y += 85.f;

    std::array<marble::physics::PhysicsBodyId, 2> bodyIds{};
    for (std::size_t i = 0u; i < 2u; ++i) {
        marble::physics::PhysicsDynamicSphereDesc desc{};
        desc.center = localGameplayToJolt(island, marbles[i].position);
        desc.linearVelocity = marbles[i].linearVelocity;
        desc.radius = marble::garden::kMarbleRadius;
        desc.invMass = marbles[i].invMass;
        desc.material.restitution = 0.672f;
        desc.material.friction = 0.42f;
        desc.material.linearDamping = 0.02f;
        desc.material.angularDamping = 0.10f;
        desc.enhancedInternalEdgeRemoval = true;
        bodyIds[i] = physics->addDynamicSphere(desc);
        if (bodyIds[i] == marble::physics::kInvalidPhysicsBodyId) {
            return 3;
        }
    }
    physics->optimizeBroadPhase();

    marble::physics::PhysicsWorldSettings const world = marble::garden::gardenAuthorityPhysicsWorldSettings();

    std::array<float, 2> jumpHold{};
    std::array<bool, 2> jumpWas{};

    constexpr float kDt = 1.f / 60.f;
    for (int t = 0; t < 360; ++t) {
        marble::garden::gardenAuthorityFixedStep(
            marble::garden::GardenAuthorityRunMode::Dedicated,
            session,
            kDt,
            false,
            *physics,
            layout,
            std::span(marbles.data(), marbles.size()),
            std::span(bodyIds.data(), bodyIds.size()),
            marbles.size(),
            std::span(jumpHold.data(), jumpHold.size()),
            std::span(jumpWas.data(), jumpWas.size()),
            world
        );
    }

    marble::physics::RigidBodyKinematics readBack{};
    std::array<marble::physics::PhysicsBodyId, 1> id0{{bodyIds[0]}};
    physics->readBackKinematics(std::span(id0.data(), 1u), &readBack, 1u);

    float const expectedY = marbles[0].position.y;
    if (readBack.position.y < expectedY - 2.f) {
        return 4;
    }

    return 0;
}
