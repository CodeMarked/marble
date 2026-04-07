// Smoke test: verifies the garden server's physics + session loop produces valid snapshots
// via loopback transport (no real UDP needed).

#include "garden/GardenSimulation.hpp"
#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/GameTransport.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "gameplay/SnapshotInterpolator.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <memory>

using namespace marble::gameplay;
using namespace marble::physics;
using marble::math::Vec3;

static void testServerProducesSnapshots() {
    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(42u, layout);

    auto physicsScene = createJoltPhysicsScene();
    SimulationIsland island{};

    PhysicsStaticHeightFieldDesc hfDesc{};
    hfDesc.offset = localGameplayToJolt(island, layout.terrain.origin);
    hfDesc.scale = {layout.terrain.cellSize, 1.f, layout.terrain.cellSize};
    hfDesc.sampleCount = layout.terrain.sampleCount;
    hfDesc.heights = std::span<float const>(layout.terrain.heights.data(), layout.terrain.heights.size());
    physicsScene->addStaticHeightField(hfDesc);

    std::array<RigidBodyKinematics, 2> marbles{};
    std::array<PhysicsBodyId, 2> bodyIds{};
    marble::garden::placeMarblesInArena(marbles, layout);

    for (std::size_t i = 0; i < 2; ++i) {
        PhysicsDynamicSphereDesc desc{};
        desc.center = localGameplayToJolt(island, marbles[i].position);
        desc.radius = marble::garden::kMarbleRadius;
        desc.invMass = 1.f / marble::garden::kPlayerBallMassKg;
        bodyIds[i] = physicsScene->addDynamicSphere(desc);
        assert(bodyIds[i] != kInvalidPhysicsBodyId);
    }
    physicsScene->optimizeBroadPhase();

    constexpr PeerId kServerId = 1u;
    constexpr PeerId kClientId = 2u;
    using Loopback = LoopbackTransportPair<2048, 64>;
    Loopback transport(kServerId, kClientId);

    AuthoritativeSession<> server{};
    SessionConfig config{};
    config.mode = MultiplayerMode::DedicatedServer;
    config.maxPlayers = 2u;
    config.simulationHz = 60u;
    config.snapshotHz = 20u;
    assert(server.initialize(config, &transport.a));

    ClientSession<> client{};
    assert(client.initialize(&transport.b, kServerId, 60u, 0xBEEFu));

    PhysicsWorldSettings worldSettings{};
    worldSettings.gravity = {0.f, -9.81f, 0.f};
    constexpr float kDt = 1.f / 60.f;

    for (int tick = 0; tick < 300; ++tick) {
        std::span<PhysicsBodyId const> bodySpan(bodyIds.data(), 2);
        physicsScene->syncHostVelocitiesBeforeStep(bodySpan, marbles.data(), 2);
        physicsScene->step(kDt, worldSettings);
        physicsScene->readBackKinematics(bodySpan, marbles.data(), 2);

        for (std::size_t i = 0; i < 2; ++i) {
            static_cast<void>(server.setEntity(
                i,
                WorldObjectRef{static_cast<std::uint64_t>(0x1000u + i)},
                marbles[i].position,
                marbles[i].linearVelocity
            ));
        }

        server.tick(kDt);
        client.tick(kDt);
    }

    assert(client.state() == ConnectionState::Connected);
    std::printf("  client connected: peer=%u\n", client.assignedPeerId());

    assert(client.snapshotCount() > 0u);
    std::printf("  snapshots received: %zu\n", client.snapshotCount());

    EntityKinematicsSnapshot snap{};
    std::size_t const entityCount = client.readLatestEntities(&snap, 1);
    assert(entityCount > 0u);
    std::printf("  latest snapshot: simTick=%u, entity=0x%llx, pos=(%.2f, %.2f, %.2f)\n",
        snap.simTick,
        static_cast<unsigned long long>(snap.entity.guid),
        static_cast<double>(snap.positionLocal.x),
        static_cast<double>(snap.positionLocal.y),
        static_cast<double>(snap.positionLocal.z));
}

static void testServerWithInterpolation() {
    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(42u, layout);

    auto physicsScene = createJoltPhysicsScene();
    SimulationIsland island{};

    PhysicsStaticHeightFieldDesc hfDesc{};
    hfDesc.offset = localGameplayToJolt(island, layout.terrain.origin);
    hfDesc.scale = {layout.terrain.cellSize, 1.f, layout.terrain.cellSize};
    hfDesc.sampleCount = layout.terrain.sampleCount;
    hfDesc.heights = std::span<float const>(layout.terrain.heights.data(), layout.terrain.heights.size());
    physicsScene->addStaticHeightField(hfDesc);

    std::array<RigidBodyKinematics, 2> marbles{};
    std::array<PhysicsBodyId, 2> bodyIds{};
    marble::garden::placeMarblesInArena(marbles, layout);

    for (std::size_t i = 0; i < 2; ++i) {
        PhysicsDynamicSphereDesc desc{};
        desc.center = localGameplayToJolt(island, marbles[i].position);
        desc.radius = marble::garden::kMarbleRadius;
        desc.invMass = 1.f / marble::garden::kPlayerBallMassKg;
        bodyIds[i] = physicsScene->addDynamicSphere(desc);
    }
    physicsScene->optimizeBroadPhase();

    constexpr PeerId kServerId = 1u;
    constexpr PeerId kClientId = 2u;
    using Loopback = LoopbackTransportPair<2048, 64>;
    Loopback transport(kServerId, kClientId);

    AuthoritativeSession<> server{};
    SessionConfig config{};
    config.mode = MultiplayerMode::DedicatedServer;
    config.maxPlayers = 2u;
    config.simulationHz = 60u;
    config.snapshotHz = 20u;
    assert(server.initialize(config, &transport.a));

    ClientSession<> client{};
    assert(client.initialize(&transport.b, kServerId, 60u, 0xCAFEu));

    SnapshotInterpolator<> interp{};
    interp.setRenderDelayTicks(4u);

    PhysicsWorldSettings worldSettings{};
    worldSettings.gravity = {0.f, -9.81f, 0.f};
    constexpr float kDt = 1.f / 60.f;
    std::size_t lastSnapCount = 0u;

    for (int tick = 0; tick < 300; ++tick) {
        std::span<PhysicsBodyId const> bodySpan(bodyIds.data(), 2);
        physicsScene->syncHostVelocitiesBeforeStep(bodySpan, marbles.data(), 2);
        physicsScene->step(kDt, worldSettings);
        physicsScene->readBackKinematics(bodySpan, marbles.data(), 2);

        for (std::size_t i = 0; i < 2; ++i) {
            static_cast<void>(server.setEntity(
                i,
                WorldObjectRef{static_cast<std::uint64_t>(0x1000u + i)},
                marbles[i].position,
                marbles[i].linearVelocity
            ));
        }

        server.tick(kDt);
        client.tick(kDt);

        std::size_t const currentSnaps = client.snapshotCount();
        if (currentSnaps > lastSnapCount) {
            auto const* entry = client.snapshotAt(0);
            if (entry != nullptr && entry->valid) {
                std::array<EntityKinematicsSnapshot, 16> snaps{};
                std::size_t count = 0u;
                std::size_t offset = 0u;
                while (offset + kEntityKinematicsSnapshotWireBytes <= entry->payloadLen && count < snaps.size()) {
                    if (!readEntityKinematicsSnapshot(entry->payload.data() + offset,
                                                      entry->payloadLen - offset, snaps[count])) {
                        break;
                    }
                    offset += kEntityKinematicsSnapshotWireBytes;
                    ++count;
                }
                if (count > 0u) {
                    interp.pushSnapshot(snaps[0].simTick, snaps.data(), count);
                }
            }
            lastSnapCount = currentSnaps;
        }
    }

    assert(interp.frameCount() > 2u);
    std::uint32_t const renderTick = interp.suggestRenderTick();
    std::array<InterpolatedEntity, 16> result{};
    std::size_t const interpCount = interp.interpolate(renderTick, result.data(), result.size());
    assert(interpCount > 0u);
    std::printf("  interpolation: %zu entities at renderTick=%u, pos=(%.2f, %.2f, %.2f) extrap=%d\n",
        interpCount,
        renderTick,
        static_cast<double>(result[0].position.x),
        static_cast<double>(result[0].position.y),
        static_cast<double>(result[0].position.z),
        result[0].extrapolated ? 1 : 0);
}

int main() {
    std::printf("garden_server_smoke_test\n");

    std::printf("testServerProducesSnapshots:\n");
    testServerProducesSnapshots();

    std::printf("testServerWithInterpolation:\n");
    testServerWithInterpolation();

    std::printf("All garden_server_smoke_test tests passed.\n");
    return 0;
}
