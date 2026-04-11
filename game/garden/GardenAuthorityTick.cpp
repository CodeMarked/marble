#include "garden/GardenAuthorityTick.hpp"

#include "garden/GardenMarblePlayer.hpp"
#include "garden/GardenSimulation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace marble::garden {

using marble::gameplay::PeerId;
using marble::gameplay::SimulationIsland;
using marble::physics::IPhysicsScene;
using marble::physics::PhysicsBodyId;
using marble::physics::PhysicsCylindricalXZClamp;
using marble::physics::PhysicsDynamicSphereDesc;
using marble::physics::PhysicsStaticBoxDesc;
using marble::physics::PhysicsStaticHeightFieldDesc;
using marble::physics::PhysicsStepOptions;
using marble::physics::PhysicsWorldSettings;
using marble::physics::RigidBodyKinematics;

std::size_t gardenServerClientMarbleCount(std::uint16_t maxPlayersRoster) noexcept {
    if (maxPlayersRoster < 2u) {
        return 1u;
    }
    std::size_t const n = static_cast<std::size_t>(maxPlayersRoster);
    return std::min(n, static_cast<std::size_t>(8u));
}

void fillGardenServerMarbleSpawnStates(std::span<RigidBodyKinematics> marbles, GardenLayout const& layout) noexcept {
    if (marbles.empty()) {
        return;
    }
    std::size_t const count = marbles.size();
    if (count <= 2u) {
        std::array<RigidBodyKinematics, 2> pair{};
        placeMarblesInArena(pair, layout);
        for (std::size_t i = 0u; i < count; ++i) {
            marbles[i] = pair[i];
        }
        return;
    }
    std::array<RigidBodyKinematics, 2> pair{};
    placeMarblesInArena(pair, layout);
    marbles[0] = pair[0];
    marbles[1] = pair[1];
    float const invMass = 1.f / kPlayerBallMassKg;
    std::size_t const ringCount = count - 2u;
    for (std::size_t i = 2u; i < count; ++i) {
        float const t = 6.2831853f * static_cast<float>(i - 2u) / static_cast<float>(ringCount);
        float const r = kArenaRadius * 0.55f;
        float const x = std::cos(t) * r;
        float const z = std::sin(t) * r;
        float const y = gardenTerrainHeightAt(layout.terrain, x, z) + kMarbleRadius + 0.12f;
        marbles[i] = RigidBodyKinematics{{x, y, z}, {}, invMass};
    }
}

void gardenAuthorityPopulateStaticCollidersFromLayout(
    IPhysicsScene& scene,
    GardenLayout const& layout,
    SimulationIsland const& island
) {
    PhysicsStaticHeightFieldDesc hfDesc{};
    hfDesc.offset = localGameplayToJolt(island, layout.terrain.origin);
    hfDesc.scale = {layout.terrain.cellSize, 1.f, layout.terrain.cellSize};
    hfDesc.sampleCount = layout.terrain.sampleCount;
    hfDesc.heights = std::span<float const>(layout.terrain.heights.data(), layout.terrain.heights.size());
    hfDesc.material.restitution = 0.35f;
    hfDesc.material.friction = 0.7f;
    static_cast<void>(scene.addStaticHeightField(hfDesc));

    for (std::size_t i = 0u; i < layout.staticColliders.size(); ++i) {
        PhysicsStaticBoxDesc boxDesc{};
        boxDesc.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
        boxDesc.material.restitution = 0.25f;
        boxDesc.material.friction = 0.6f;
        static_cast<void>(scene.addStaticBox(boxDesc));
    }
}

void gardenAuthorityFixedStep(
    GardenAuthorityRunMode mode,
    marble::gameplay::AuthoritativeSession<>& session,
    float fixedDt,
    bool useAoi,
    IPhysicsScene& physicsScene,
    GardenLayout const& layout,
    std::span<RigidBodyKinematics> marbles,
    std::span<PhysicsBodyId const> marbleBodyIds,
    std::size_t marbleCount,
    std::span<float> jumpHoldSeconds,
    std::span<bool> jumpWasHeld,
    PhysicsWorldSettings const& worldSettings
) {
    constexpr PeerId kClientPeerScanEnd =
        static_cast<PeerId>(2u + marble::gameplay::UdpGameTransport::kMaxPeers);

    if (useAoi && session.peerCount() > 0u) {
        for (PeerId peer = 2u; peer < kClientPeerScanEnd; ++peer) {
            if (session.isConnected(peer)) {
                std::size_t const slot = static_cast<std::size_t>(peer) - 2u;
                if (slot < marbleCount) {
                    session.setPeerViewEntity(peer, slot);
                }
            }
        }
    }

    session.tick(fixedDt);

    if (mode == GardenAuthorityRunMode::Dedicated && session.peerCount() == 0u) {
        return;
    }

    for (PeerId peer = 2u; peer < kClientPeerScanEnd; ++peer) {
        auto const* inp = session.latestInput(peer);
        if (inp == nullptr) {
            continue;
        }
        bool const listenSyntheticHost =
            mode == GardenAuthorityRunMode::ListenHost && peer == 2u;
        if (!session.isConnected(peer) && !listenSyntheticHost) {
            session.clearInput(peer);
            continue;
        }
        std::size_t const slot = static_cast<std::size_t>(peer) - 2u;
        if (slot >= marbleCount) {
            session.clearInput(peer);
            continue;
        }
        if (slot >= jumpHoldSeconds.size() || slot >= jumpWasHeld.size()) {
            session.clearInput(peer);
            continue;
        }

        bool const jumpHeld = (inp->buttons & marble::gameplay::kClientInputButton_Jump) != 0;
        applyGardenMarblePlayerStep(
            marbles[slot],
            layout,
            inp->moveX,
            inp->moveZ,
            jumpHeld,
            jumpWasHeld[slot],
            jumpHoldSeconds[slot],
            fixedDt,
            kMarbleRadius);

        session.clearInput(peer);
    }

    std::span<PhysicsBodyId const> bodySpan(marbleBodyIds.data(), marbleCount);
    physicsScene.syncHostVelocitiesBeforeStep(bodySpan, marbles.data(), marbleCount);

    PhysicsCylindricalXZClamp clamp{};
    clamp.maxHorizontalRadiusFromYAxis = kGardenRadius - kMarbleRadius - 0.02f;
    clamp.minCenterY = -5.f;
    PhysicsStepOptions stepOpts{};
    stepOpts.postStepCylindricalClamp = &clamp;
    stepOpts.clampBodyIds = bodySpan;

    physicsScene.step(fixedDt, worldSettings, stepOpts);
    physicsScene.readBackKinematics(bodySpan, marbles.data(), marbleCount);

    for (std::size_t i = 0u; i < marbleCount; ++i) {
        static_cast<void>(session.setEntity(
            i,
            marble::gameplay::WorldObjectRef{static_cast<std::uint64_t>(0x1000u + i)},
            marbles[i].position,
            marbles[i].linearVelocity,
            marble::gameplay::PhysicsSimulationTier::Contact,
            0.f
        ));
    }
}

} // namespace marble::garden
