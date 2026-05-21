#include "garden/detail/GardenGameState.hpp"
#include "garden/GardenGameHelpers.hpp"
#include "garden/GardenAuthorityTick.hpp"
#include "garden/GardenSimulation.hpp"

#include <cstdio>
#include <filesystem>

namespace marble::garden_app {

using namespace marble::gameplay;
using namespace marble::physics;
using namespace marble::garden;
using namespace marble::input;
using namespace detail;
void GardenGame::State::rebuildPhysicsFromLayout() noexcept {
    physicsScene_->clear();
    PhysicsBodyMaterial staticMat{};
    staticMat.restitution = 0.30f;
    staticMat.friction = 0.55f;
    GardenTerrain const& tr = layout.terrain;
    if (tr.sampleCount >= 2u && tr.heights.size() == static_cast<std::size_t>(tr.sampleCount) * tr.sampleCount) {
        PhysicsStaticHeightFieldDesc hf{};
        hf.offset = tr.origin;
        hf.scale = {tr.cellSize, 1.f, tr.cellSize};
        hf.sampleCount = tr.sampleCount;
        hf.heights = std::span<float const>(tr.heights.data(), tr.heights.size());
        hf.material = staticMat;
        PhysicsStaticHeightFieldDesc const hfJ = localGameplayHeightFieldToJolt(island_, hf);
        (void)physicsScene_->addStaticHeightField(hfJ);
    }
    gardenAddStaticPropBodiesFromLayout(*physicsScene_, layout, island_, staticMat);
    PhysicsBodyMaterial marbleMat{};
    marbleMat.restitution = 0.672f;
    marbleMat.friction = 0.42f;
    marbleMat.linearDamping = 0.02f;
    marbleMat.angularDamping = 0.10f;
    for (std::size_t i = 0; i < marbleCount_; ++i) {
        PhysicsDynamicSphereDesc sd{};
        sd.center = localGameplayToJolt(island_, marbles[i].position);
        sd.linearVelocity = marbles[i].linearVelocity;
        sd.radius = kMarbleRadius;
        sd.invMass = marbles[i].invMass;
        sd.enhancedInternalEdgeRemoval = true;
        sd.material = marbleMat;
        marbleBodyIds_[i] = physicsScene_->addDynamicSphere(sd);
    }
    physicsScene_->optimizeBroadPhase();
}

void GardenGame::State::rebuildListenHostDynamics() noexcept {
    using marble::physics::PhysicsBodyId;
    using marble::physics::PhysicsDynamicSphereDesc;
    for (std::size_t i = 0u; i < marbleCount_; ++i) {
        PhysicsDynamicSphereDesc desc{};
        desc.center = localGameplayToJolt(island_, marbles[i].position);
        desc.linearVelocity = marbles[i].linearVelocity;
        desc.radius = kMarbleRadius;
        desc.invMass = 1.f / marble::garden::kPlayerBallMassKg;
        desc.material.restitution = 0.672f;
        desc.material.friction = 0.42f;
        desc.material.linearDamping = 0.02f;
        desc.material.angularDamping = 0.10f;
        desc.enhancedInternalEdgeRemoval = true;
        marbleBodyIds_[i] = physicsScene_->addDynamicSphere(desc);
    }
    physicsScene_->optimizeBroadPhase();
}

void GardenGame::State::rebuildListenHostPhysicsScene() noexcept {
    physicsScene_->clear();
    marble::garden::gardenAuthorityPopulateStaticCollidersFromLayout(*physicsScene_, layout, island_);
    rebuildListenHostDynamics();
}

void GardenGame::State::initListenHostNetworking() noexcept {
    hostTransport_ = std::make_unique<UdpGameTransport>();
    std::uint16_t port = 27778u;
    if (char const* envPort = std::getenv("GARDEN_LISTEN_PORT")) {
        char* end{};
        unsigned long const v = std::strtoul(envPort, &end, 10);
        if (end != envPort && *end == '\0' && v > 0ul && v <= 65535ul) {
            port = static_cast<std::uint16_t>(v);
        }
    }
    if (!hostTransport_->bind(port)) {
        std::fprintf(stderr, "GardenGame: listen host failed to bind UDP port %u\n", static_cast<unsigned>(port));
        return;
    }
    hostTransport_->setMinimumJoinerPeerId(3u);
    hostListenPort_ = hostTransport_->localPort();
    if (!hostSession_.initialize(sessionConfig_, hostTransport_.get())) {
        std::fprintf(stderr, "GardenGame: listen host AuthoritativeSession init failed\n");
        hostTransport_.reset();
        return;
    }
    hostSession_.setFullSnapshotWhenActiveEntityCountAtMost(
        static_cast<std::uint32_t>(marble::garden::kMaxGardenAuthorityMarbles));
    hostSession_.setAoiEnabled(true);
    hostSession_.setDefaultAoiRadius(2500.f);
    hostSession_.setAoiEntityVelocityLookaheadSeconds(0.5f);
    hostSession_.setAoiViewerPositionLookaheadSeconds(0.f);
    hostListenInitialized_ = true;
    std::fprintf(
        stderr,
        "GardenGame: listen host UDP port %u — remotes use layout seed 0x%X; first joiner peer id 3 (host "
        "synthetic 2)\n",
        static_cast<unsigned>(hostListenPort_),
        static_cast<unsigned>(kLayoutSeed));
}

void GardenGame::State::fallbackFromListenHostToOffline() noexcept {
    sessionKind_ = GardenSessionKind::Offline;
    hostListenInitialized_ = false;
    hostAuthorityFixedAccum_ = 0.f;
    hostTransport_.reset();
    sessionConfig_ = SessionConfig{
        MultiplayerMode::Offline,
        1u,
        60u,
        20u,
        2u,
        0u,
        1400u,
    };
    (void)roster_.bootstrap(MultiplayerMode::Offline);
    marbleCount_ = 2u;
    std::array<RigidBodyKinematics, 2> pair{};
    placeMarblesInArena(pair, layout);
    marbles[0] = pair[0];
    marbles[1] = pair[1];
    physics.setSettings(gardenAuthorityPhysicsWorldSettings());
    rebuildPhysicsFromLayout();
}

GardenGame::State::State(marble::core::Engine& e, GardenSessionKind session, RemoteClientParams params)
    : engine(e), sessionKind_(session), clientParams_(std::move(params)) {
    meshProps.fill(std::numeric_limits<std::uint32_t>::max());
    if (sessionKind_ == GardenSessionKind::ListenHost) {
        sessionConfig_ = SessionConfig{
            MultiplayerMode::ListenServer,
            4u,
            60u,
            20u,
            2u,
            0u,
            1400u,
        };
        (void)roster_.bootstrap(MultiplayerMode::ListenServer);
    } else {
        sessionConfig_ = SessionConfig{
            MultiplayerMode::Offline,
            1u,
            60u,
            20u,
            2u,
            0u,
            1400u,
        };
        (void)roster_.bootstrap(MultiplayerMode::Offline);
    }
    if (sessionKind_ != GardenSessionKind::RemoteClient && !isValid(sessionConfig_)) {
        sessionConfig_ = SessionConfig{};
        (void)roster_.bootstrap(MultiplayerMode::Offline);
        sessionKind_ = GardenSessionKind::Offline;
    }
    buildGardenLayout(layoutSeedForCurrentSession(), layout);

    if (sessionKind_ == GardenSessionKind::RemoteClient) {
        marbleCount_ = 2u;
        std::array<RigidBodyKinematics, 2> pair{};
        placeMarblesInArena(pair, layout);
        marbles[0] = pair[0];
        marbles[1] = pair[1];
        initClientNetworking();
    } else if (sessionKind_ == GardenSessionKind::ListenHost) {
        marbleCount_ = marble::garden::gardenServerClientMarbleCount(sessionConfig_.maxPlayers);
        marble::garden::fillGardenServerMarbleSpawnStates(
            std::span<RigidBodyKinematics>(marbles.data(), marbleCount_), layout);
        hostAuthorityWorld_ = gardenAuthorityPhysicsWorldSettings();
        rebuildListenHostPhysicsScene();
        initListenHostNetworking();
        if (!hostListenInitialized_) {
            std::fprintf(stderr, "GardenGame: reverting listen host to offline Garden\n");
            fallbackFromListenHostToOffline();
        }
    } else {
        marbleCount_ = 2u;
        std::array<RigidBodyKinematics, 2> pair{};
        placeMarblesInArena(pair, layout);
        marbles[0] = pair[0];
        marbles[1] = pair[1];
        physics.setSettings(gardenAuthorityPhysicsWorldSettings());
        rebuildPhysicsFromLayout();
    }

    (void)inputRemap_.bind(AbstractControl::BackSelect, {kActionPauseMenu, ControlValueClass::DigitalButton, false});

    inputPolicy_.resetActionGates();
    inputPolicy_.applyActionContext(std::span{kGameplayContext});
}

void GardenGame::State::initClientNetworking() noexcept {
    clientTransport_ = std::make_unique<UdpGameTransport>();
    if (!clientTransport_->bind(0u)) {
        std::fprintf(stderr, "GardenGame: failed to bind client UDP socket\n");
        sessionKind_ = GardenSessionKind::Offline;
        physics.setSettings(gardenAuthorityPhysicsWorldSettings());
        rebuildPhysicsFromLayout();
        return;
    }
    constexpr PeerId kServerPeerId = 1u;
    if (!clientTransport_->addPeer(kServerPeerId, clientParams_.host.c_str(), clientParams_.port)) {
        std::fprintf(stderr, "GardenGame: failed to add server peer\n");
        sessionKind_ = GardenSessionKind::Offline;
        physics.setSettings(gardenAuthorityPhysicsWorldSettings());
        rebuildPhysicsFromLayout();
        return;
    }
    if (!clientSession_.initialize(
            clientTransport_.get(), kServerPeerId, 60u, 0xCAFEu, clientParams_.joinTokenU32)) {
        std::fprintf(stderr, "GardenGame: failed to initialize client session\n");
        sessionKind_ = GardenSessionKind::Offline;
        physics.setSettings(gardenAuthorityPhysicsWorldSettings());
        rebuildPhysicsFromLayout();
        return;
    }
    snapInterp_.setRenderDelayTicks(4u);
    remoteNetTuning_ = gardenRemoteNetTuningFromEnv();
    gardenRemoteApplyInterpTunables(snapInterp_, remoteNetTuning_);
    if (remoteNetTuning_.noPredict) {
        std::fprintf(
            stderr,
            "GardenGame: GARDEN_REMOTE_NO_PREDICT=1 — local pod prediction OFF (compare feel vs default).\n");
    }
    if (remoteNetTuning_.desyncLog) {
        std::fprintf(
            stderr,
            "GardenGame: GARDEN_REMOTE_DESYNC_LOG=1 — stderr metrics every %d ms "
            "(override with GARDEN_REMOTE_DESYNC_LOG_MS).\n",
            remoteNetTuning_.desyncLogPeriodMs);
    }
    if (remoteNetTuning_.interpLog) {
        std::fprintf(
            stderr,
            "GardenGame: GARDEN_REMOTE_INTERP_LOG=1 — stderr interp metrics every %d ms "
            "(override with GARDEN_REMOTE_INTERP_LOG_MS).\n",
            remoteNetTuning_.interpLogPeriodMs);
    }
}

void GardenGame::State::primeAssetRegistryOnce() {
    if (assetRegistryPrimed_) {
        return;
    }
    assetRegistryPrimed_ = true;
    std::string const root = engine.assetsRootPath();
    if (root.empty()) {
        return;
    }
    (void)marble::core::setBinaryResourceSearchRoot(assetRegistry_, std::filesystem::path(root));
    (void)assetRegistry_.acquire("README.txt");
}

void GardenGame::State::restartGarden() noexcept {
    buildGardenLayout(layoutSeedForCurrentSession(), layout);
    if (sessionKind_ == GardenSessionKind::RemoteClient) {
        marbleCount_ = 2u;
        std::array<RigidBodyKinematics, 2> pair{};
        placeMarblesInArena(pair, layout);
        marbles[0] = pair[0];
        marbles[1] = pair[1];
        snapInterp_.reset();
        gardenRemoteApplyInterpTunables(snapInterp_, remoteNetTuning_);
        remoteLastConsumedSnapshotServerTick_.reset();
        remoteAuthMarbleValid_.fill(false);
        remoteAuthMarbleYawValid_.fill(false);
        remoteDisplayYaw_.fill(0.f);
        remoteMarbleReplTier_.fill(marble::gameplay::PhysicsSimulationTier::Contact);
        lastAckedServerSimTick_ = 0u;
        remoteInterpDelaySmoothed_ = 0u;
    } else if (sessionKind_ == GardenSessionKind::ListenHost && hostListenInitialized_) {
        marbleCount_ = marble::garden::gardenServerClientMarbleCount(sessionConfig_.maxPlayers);
        marble::garden::fillGardenServerMarbleSpawnStates(
            std::span<RigidBodyKinematics>(marbles.data(), marbleCount_), layout);
        hostPeerJumpHoldSec_.fill(0.f);
        hostPeerJumpWasHeld_.fill(false);
        hostInputTickCounter_ = 0u;
        rebuildListenHostPhysicsScene();
    } else {
        marbleCount_ = 2u;
        std::array<RigidBodyKinematics, 2> pair{};
        placeMarblesInArena(pair, layout);
        marbles[0] = pair[0];
        marbles[1] = pair[1];
        rebuildPhysicsFromLayout();
    }
    camYaw = camYawTarget = 0.7f;
    camDist = camDistTarget = 14.f;
    camHeight = camHeightTarget = 3.6f;
    mouseFlickArmed = false;
    hasPlaneStroke = false;
    padFlickArmed = false;
    strokeEmaValid = false;
    padStrokeEmaValid = false;
    jumpChargeSec_ = 0.f;
    jumpWasHeld_ = false;
    paused = false;
    pausePanel = PausePanel::Main;
}

/// Orbit camera / local marble highlight: slot `peerId - 2` when remote and connected (matches server mapping).
[[nodiscard]] std::size_t GardenGame::State::localViewMarbleIndex() const noexcept {
    if (sessionKind_ != GardenSessionKind::RemoteClient) {
        return 0u;
    }
    if (clientSession_.state() != ConnectionState::Connected) {
        return 0u;
    }
    PeerId const ap = clientSession_.assignedPeerId();
    if (ap < 2u) {
        return 0u;
    }
    std::size_t const slot = static_cast<std::size_t>(ap) - 2u;
    return slot < marbleCount_ ? slot : 0u;
}

void GardenGame::State::tickRemoteClient(float dt) noexcept {
    if (!clientTransport_) {
        return;
    }
    clientSession_.tick(dt);

    if (clientSession_.state() == ConnectionState::Connected && !clientConnected_) {
        clientConnected_ = true;
        snapInterp_.reset();
        gardenRemoteApplyInterpTunables(snapInterp_, remoteNetTuning_);
        remoteLastConsumedSnapshotServerTick_.reset();
        remoteAuthMarbleValid_.fill(false);
        remoteAuthMarbleYawValid_.fill(false);
        remoteDisplayYaw_.fill(0.f);
        remoteMarbleReplTier_.fill(marble::gameplay::PhysicsSimulationTier::Contact);
        lastAckedServerSimTick_ = 0u;
        jumpChargeSec_ = 0.f;
        jumpWasHeld_ = false;
        lastDesyncLogTime_.reset();
        lastInterpLogTime_.reset();
        desyncPosSnapCount_ = 0u;
        remoteClientPredictAccum_ = 0.f;
        remoteInterpDelaySmoothed_ = 0u;
    }
    if (clientSession_.state() == ConnectionState::Disconnected) {
        clientConnected_ = false;
        remoteClientPredictAccum_ = 0.f;
        remoteInterpDelaySmoothed_ = 0u;
        remoteMarbleReplTier_.fill(marble::gameplay::PhysicsSimulationTier::Contact);
        remoteLastConsumedSnapshotServerTick_.reset();
        remoteAuthMarbleValid_.fill(false);
        remoteAuthMarbleYawValid_.fill(false);
        remoteDisplayYaw_.fill(0.f);
        jumpChargeSec_ = 0.f;
        jumpWasHeld_ = false;
    }

    SessionStateCorrectionPayload corr{};
    while (clientSession_.takePendingStateCorrection(corr)) {
        if (corr.entity.guid >= 0x1000u) {
            std::size_t const idx = static_cast<std::size_t>(corr.entity.guid - 0x1000u);
            if (idx < remoteAuthMarblePos_.size()) {
                remoteAuthMarblePos_[idx] = corr.positionLocal;
                remoteAuthMarbleValid_[idx] = true;
                remoteAuthMarbleYaw_[idx] = corr.yawRadians;
                remoteAuthMarbleYawValid_[idx] = true;
            }
        }
    }

    // Ingest every buffered snapshot with a strictly newer `simTick`, oldest ring entry first, so the
    // interpolator receives monotonic keyframes (reading only `snapshotAt(0)` drops intermediate ticks).
    if (clientSession_.state() == ConnectionState::Connected && clientSession_.snapshotCount() > 0u) {
        std::size_t const snapN = clientSession_.snapshotCount();
        for (std::size_t ringIdx = 0u; ringIdx < snapN; ++ringIdx) {
            std::size_t const i = snapN - 1u - ringIdx;
            auto const* entry = clientSession_.snapshotAt(i);
            if (entry == nullptr || !entry->valid || entry->payloadLen < kEntityKinematicsSnapshotWireBytes) {
                continue;
            }
            EntityKinematicsSnapshot probe{};
            if (!readEntityKinematicsSnapshot(entry->payload.data(), entry->payloadLen, probe)) {
                continue;
            }
            bool const newer = !remoteLastConsumedSnapshotServerTick_.has_value() ||
                probe.simTick > *remoteLastConsumedSnapshotServerTick_;
            if (!newer) {
                continue;
            }
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
                remoteLastConsumedSnapshotServerTick_ = probe.simTick;
                snapInterp_.pushSnapshot(snaps[0].simTick, snaps.data(), count);
                clientSession_.noteAuthoritativeSnapshot(snaps[0].simTick);
                lastAckedServerSimTick_ = snaps[0].simTick;
                for (std::size_t ei = 0u; ei < count; ++ei) {
                    std::uint64_t const g = snaps[ei].entity.guid;
                    if (g >= 0x1000u) {
                        std::size_t const idx = static_cast<std::size_t>(g - 0x1000u);
                        if (idx < remoteAuthMarblePos_.size()) {
                            remoteAuthMarblePos_[idx] = snaps[ei].positionLocal;
                            remoteAuthMarbleValid_[idx] = true;
                            remoteAuthMarbleYaw_[idx] = snaps[ei].yawRadians;
                            remoteAuthMarbleYawValid_[idx] = true;
                        }
                    }
                }
            }
        }
    }

    if (clientSession_.state() == ConnectionState::Connected) {
        ClientInputWirePayload inp{};
        inp.clientTick = inputTickCounter_++;
        inp.serverTickAck = lastAckedServerSimTick_;
        inp.moveX = pendingMoveX_;
        inp.moveZ = pendingMoveZ_;
        inp.steer = 0.f;
        inp.buttons = pendingButtons_;

        std::array<std::uint8_t, kClientInputWirePayloadBytes> payload{};
        std::size_t const payloadLen = writeClientInputPayload(payload.data(), payload.size(), inp);
        if (payloadLen > 0u) {
            constexpr PeerId kServerPeerId = 1u;
            std::array<std::uint8_t, 64> frame{};
            std::size_t const frameLen = writeSessionClientInput(
                frame.data(), frame.size(), payload.data(), payloadLen);
            if (frameLen > 0u) {
                static_cast<void>(clientTransport_->send(kServerPeerId, frame.data(), frameLen));
            }
        }
    }
}
} // namespace marble::garden_app
