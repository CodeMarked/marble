// Smoke test: verifies the garden server's physics + session loop produces valid snapshots
// via loopback transport (no real UDP needed).

#include "garden/GardenMarblePlayer.hpp"
#include "garden/GardenSimulation.hpp"
#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/GameTransport.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
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
#include <cmath>
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
    static_cast<void>(physicsScene->addStaticHeightField(hfDesc));

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

/// One fixed step aligned with `GardenServerMain.cpp`: `session.tick` (receives ClientInput),
/// apply horizontal roll + `clearInput`, sync velocities, Jolt step + cylindrical clamp, read back,
/// `setEntity`, then `client.tick`.
static void gardenServerStyleFixedStep(
    AuthoritativeSession<>& server,
    ClientSession<>& client,
    IGameTransport& clientToServerTransport,
    PeerId serverPeer,
    IPhysicsScene& physicsScene,
    marble::garden::GardenLayout const& layout,
    std::span<RigidBodyKinematics> marbles,
    std::span<PhysicsBodyId const> bodyIds,
    PhysicsWorldSettings const& worldSettings,
    float kDt,
    bool enqueueMoveX,
    std::uint32_t& clientInputSeq,
    bool& jumpWasHeld,
    float& jumpChargeSec
) {
    if (enqueueMoveX) {
        ClientInputWirePayload ci{};
        ci.clientTick = clientInputSeq++;
        ci.moveX = 1.f;
        ci.moveZ = 0.f;
        ci.buttons = 0;
        std::array<std::uint8_t, kClientInputWirePayloadBytes> pl{};
        assert(writeClientInputPayload(pl.data(), pl.size(), ci) == kClientInputWirePayloadBytes);
        std::array<std::uint8_t, 128> frame{};
        std::size_t const flen = writeSessionClientInput(frame.data(), frame.size(), pl.data(), pl.size());
        assert(flen > 0u);
        assert(clientToServerTransport.send(serverPeer, frame.data(), flen));
    }

    server.tick(kDt);

    constexpr PeerId kFirstClientPeer = 2u;
    auto const* inp = server.latestInput(kFirstClientPeer);
    if (inp != nullptr) {
        bool const jumpHeld = (inp->buttons & kClientInputButton_Jump) != 0;
        marble::garden::applyGardenMarblePlayerStep(
            marbles[0],
            layout,
            inp->moveX,
            inp->moveZ,
            jumpHeld,
            jumpWasHeld,
            jumpChargeSec,
            kDt,
            marble::garden::kMarbleRadius);
        server.clearInput(kFirstClientPeer);
    }

    physicsScene.syncHostVelocitiesBeforeStep(bodyIds, marbles.data(), marbles.size());

    PhysicsCylindricalXZClamp clamp{};
    clamp.maxHorizontalRadiusFromYAxis =
        marble::garden::kGardenRadius - marble::garden::kMarbleRadius - 0.02f;
    clamp.minCenterY = -5.f;
    PhysicsStepOptions stepOpts{};
    stepOpts.postStepCylindricalClamp = &clamp;
    stepOpts.clampBodyIds = bodyIds;

    physicsScene.step(kDt, worldSettings, stepOpts);
    physicsScene.readBackKinematics(bodyIds, marbles.data(), marbles.size());

    for (std::size_t i = 0u; i < marbles.size(); ++i) {
        static_cast<void>(server.setEntity(
            i,
            WorldObjectRef{static_cast<std::uint64_t>(0x1000u + i)},
            marbles[i].position,
            marbles[i].linearVelocity
        ));
    }

    client.tick(kDt);
}

static void testClientInputDrivesAuthoritativeMarbleAndSnapshots() {
    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(42u, layout);

    auto physicsScene = createJoltPhysicsScene();
    SimulationIsland island{};

    PhysicsStaticHeightFieldDesc hfDesc{};
    hfDesc.offset = localGameplayToJolt(island, layout.terrain.origin);
    hfDesc.scale = {layout.terrain.cellSize, 1.f, layout.terrain.cellSize};
    hfDesc.sampleCount = layout.terrain.sampleCount;
    hfDesc.heights = std::span<float const>(layout.terrain.heights.data(), layout.terrain.heights.size());
    hfDesc.material.restitution = 0.35f;
    hfDesc.material.friction = 0.7f;
    static_cast<void>(physicsScene->addStaticHeightField(hfDesc));

    for (std::size_t i = 0u; i < layout.staticColliders.size(); ++i) {
        PhysicsStaticBoxDesc boxDesc{};
        boxDesc.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
        boxDesc.material.restitution = 0.25f;
        boxDesc.material.friction = 0.6f;
        static_cast<void>(physicsScene->addStaticBox(boxDesc));
    }

    constexpr std::size_t kMarbleCount = 2u;
    std::array<RigidBodyKinematics, kMarbleCount> marbles{};
    std::array<PhysicsBodyId, kMarbleCount> bodyIds{};
    marble::garden::placeMarblesInArena(marbles, layout);

    for (std::size_t i = 0u; i < kMarbleCount; ++i) {
        PhysicsDynamicSphereDesc desc{};
        desc.center = localGameplayToJolt(island, marbles[i].position);
        desc.linearVelocity = marbles[i].linearVelocity;
        desc.radius = marble::garden::kMarbleRadius;
        desc.invMass = 1.f / marble::garden::kPlayerBallMassKg;
        desc.material.restitution = 0.45f;
        desc.material.friction = 0.5f;
        desc.material.linearDamping = 0.04f;
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
    assert(client.initialize(&transport.b, kServerId, 60u, 0xF00Du));

    PhysicsWorldSettings worldSettings{};
    worldSettings.gravity = {0.f, -9.81f, 0.f};
    worldSettings.enableSleeping = false;
    worldSettings.enableContinuousCollision = true;
    worldSettings.maxSubSteps = 1u;

    constexpr float kDt = 1.f / 60.f;
    std::uint32_t clientInputSeq = 0u;
    bool jumpWasHeld = false;
    float jumpChargeSec = 0.f;

    for (int t = 0; t < 300 && client.state() != ConnectionState::Connected; ++t) {
        std::span<RigidBodyKinematics> marbleSpan(marbles.data(), kMarbleCount);
        std::span<PhysicsBodyId const> bodySpan(bodyIds.data(), kMarbleCount);
        gardenServerStyleFixedStep(
            server,
            client,
            transport.b,
            kServerId,
            *physicsScene,
            layout,
            marbleSpan,
            bodySpan,
            worldSettings,
            kDt,
            false,
            clientInputSeq,
            jumpWasHeld,
            jumpChargeSec
        );
    }
    assert(client.state() == ConnectionState::Connected);

    for (int t = 0; t < 90; ++t) {
        std::span<RigidBodyKinematics> marbleSpan(marbles.data(), kMarbleCount);
        std::span<PhysicsBodyId const> bodySpan(bodyIds.data(), kMarbleCount);
        gardenServerStyleFixedStep(
            server,
            client,
            transport.b,
            kServerId,
            *physicsScene,
            layout,
            marbleSpan,
            bodySpan,
            worldSettings,
            kDt,
            false,
            clientInputSeq,
            jumpWasHeld,
            jumpChargeSec
        );
    }

    float const x0 = marbles[0].position.x;

    for (int t = 0; t < 420; ++t) {
        std::span<RigidBodyKinematics> marbleSpan(marbles.data(), kMarbleCount);
        std::span<PhysicsBodyId const> bodySpan(bodyIds.data(), kMarbleCount);
        gardenServerStyleFixedStep(
            server,
            client,
            transport.b,
            kServerId,
            *physicsScene,
            layout,
            marbleSpan,
            bodySpan,
            worldSettings,
            kDt,
            true,
            clientInputSeq,
            jumpWasHeld,
            jumpChargeSec
        );
    }

    assert(marbles[0].position.x > x0 + 0.2f);
    EntityKinematicsSnapshot snap{};
    assert(client.readLatestEntities(&snap, 1) == 1);
    assert(snap.positionLocal.x > x0 + 0.15f);
    std::printf("  client input -> motion: x0=%.3f x1=%.3f snap.x=%.3f\n",
        static_cast<double>(x0),
        static_cast<double>(marbles[0].position.x),
        static_cast<double>(snap.positionLocal.x));
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
    static_cast<void>(physicsScene->addStaticHeightField(hfDesc));

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

/// Mirrors `garden_server` when `--aoi-radius` is tight: per-tick `setPeerViewEntity(client, 0)` and only nearby
/// marbles replicate (dedicated path uses the same `AuthoritativeSession` AOI hooks).
static void testGardenStyleAoiFiltersDistantMarble() {
    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(99u, layout);

    auto physicsScene = createJoltPhysicsScene();
    SimulationIsland island{};

    PhysicsStaticHeightFieldDesc hfDesc{};
    hfDesc.offset = localGameplayToJolt(island, layout.terrain.origin);
    hfDesc.scale = {layout.terrain.cellSize, 1.f, layout.terrain.cellSize};
    hfDesc.sampleCount = layout.terrain.sampleCount;
    hfDesc.heights = std::span<float const>(layout.terrain.heights.data(), layout.terrain.heights.size());
    static_cast<void>(physicsScene->addStaticHeightField(hfDesc));

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

    float const y0 = marble::garden::gardenTerrainHeightAt(layout.terrain, 0.f, 0.f) + marble::garden::kMarbleRadius;
    float const y1 =
        marble::garden::gardenTerrainHeightAt(layout.terrain, 420.f, 0.f) + marble::garden::kMarbleRadius;
    Vec3 const p0{0.f, y0, 0.f};
    Vec3 const p1{420.f, y1, 0.f};
    marbles[0].position = p0;
    marbles[1].position = p1;
    marbles[0].linearVelocity = {};
    marbles[1].linearVelocity = {};
    physicsScene->setBodyCenterAndLinearVelocity(bodyIds[0], localGameplayToJolt(island, p0), Vec3{});
    physicsScene->setBodyCenterAndLinearVelocity(bodyIds[1], localGameplayToJolt(island, p1), Vec3{});

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
    assert(client.initialize(&transport.b, kServerId, 60u, 0xA01u));

    server.setAoiEnabled(true);
    server.setDefaultAoiRadius(100.f);

    PhysicsWorldSettings worldSettings{};
    worldSettings.gravity = {0.f, -9.81f, 0.f};
    constexpr float kDt = 1.f / 60.f;

    for (int tick = 0; tick < 400; ++tick) {
        if (server.isConnected(kClientId)) {
            server.setPeerViewEntity(kClientId, 0u);
        }

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

    std::array<EntityKinematicsSnapshot, 4> snaps{};
    std::size_t const aoiCount = client.readLatestEntities(snaps.data(), snaps.size());
    assert(aoiCount == 1u);
    assert(snaps[0].entity.guid == 0x1000u);
    std::printf("  AOI on: latest entity count=%zu (expect 1)\n", aoiCount);

    server.setAoiEnabled(false);
    for (int i = 0; i < 12; ++i) {
        std::span<PhysicsBodyId const> bodySpan(bodyIds.data(), 2);
        physicsScene->syncHostVelocitiesBeforeStep(bodySpan, marbles.data(), 2);
        physicsScene->step(kDt, worldSettings);
        physicsScene->readBackKinematics(bodySpan, marbles.data(), 2);
        for (std::size_t e = 0; e < 2; ++e) {
            static_cast<void>(server.setEntity(
                e,
                WorldObjectRef{static_cast<std::uint64_t>(0x1000u + e)},
                marbles[e].position,
                marbles[e].linearVelocity
            ));
        }
        server.tick(kDt);
        client.tick(kDt);
    }

    std::size_t const fullCount = client.readLatestEntities(snaps.data(), snaps.size());
    assert(fullCount == 2u);
    std::printf("  AOI off: latest entity count=%zu (expect 2)\n", fullCount);
}

int main() {
    std::printf("garden_server_smoke_test\n");

    std::printf("testServerProducesSnapshots:\n");
    testServerProducesSnapshots();

    std::printf("testClientInputDrivesAuthoritativeMarbleAndSnapshots:\n");
    testClientInputDrivesAuthoritativeMarbleAndSnapshots();

    std::printf("testServerWithInterpolation:\n");
    testServerWithInterpolation();

    std::printf("testGardenStyleAoiFiltersDistantMarble:\n");
    testGardenStyleAoiFiltersDistantMarble();

    std::printf("All garden_server_smoke_test tests passed.\n");
    return 0;
}
