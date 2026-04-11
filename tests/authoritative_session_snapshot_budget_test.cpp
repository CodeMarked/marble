#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"

// maxSnapshotBytesPerPeer caps concatenated kinematics payload length; at least one entity still sent.

int main() {
    using namespace marble::gameplay;

    static constexpr PeerId kServerPeerId = 1u;
    static constexpr PeerId kClientPeerId = 2u;
    static constexpr std::size_t kMaxPayload = 2048;
    static constexpr std::size_t kQueueDepth = 64;

    LoopbackTransportPair<kMaxPayload, kQueueDepth> pair{kServerPeerId, kClientPeerId};

    AuthoritativeSession<8> server{};
    SessionConfig cfg{};
    cfg.mode = MultiplayerMode::DedicatedServer;
    cfg.maxPlayers = 8u;
    cfg.simulationHz = 60u;
    cfg.snapshotHz = 20u;
    cfg.maxPredictionTicks = 2u;
    cfg.maxSnapshotBytesPerPeer = 88u;

    if (!server.initialize(cfg, &pair.a)) {
        return 1;
    }

    ClientSession<32, 8> client{};
    if (!client.initialize(&pair.b, kServerPeerId, 60u, 0x51EEu)) {
        return 2;
    }

    float const dt = 1.0f / 60.0f;
    client.tick(dt);
    server.tick(dt);
    client.tick(dt);
    if (client.state() != ConnectionState::Connected) {
        return 3;
    }

    server.setAoiEnabled(false);

    for (std::size_t i = 0u; i < AuthoritativeSession<8>::kMaxEntities; ++i) {
        if (!server.setEntity(
                i,
                {0xF000u + static_cast<std::uint64_t>(i)},
                {static_cast<float>(i) * 8.f, 0.f, 0.f},
                {0.5f, 0.f, 0.f},
                PhysicsSimulationTier::Contact)) {
            return 4;
        }
    }
    server.setPeerViewEntity(kClientPeerId, 0u);

    std::size_t maxPayloadObserved = 0u;
    std::size_t maxEntitiesObserved = 0u;
    bool sawSnapshot = false;

    for (int step = 0; step < 200; ++step) {
        std::uint32_t const tick = server.currentTick();
        for (std::size_t i = 0u; i < AuthoritativeSession<8>::kMaxEntities; ++i) {
            float const jitter = static_cast<float>(tick) * 0.0001f + static_cast<float>(step) * 0.00001f;
            static_cast<void>(server.setEntity(
                i,
                {0xF000u + static_cast<std::uint64_t>(i)},
                {static_cast<float>(i) * 8.f + jitter, 0.f, 0.f},
                {0.5f, 0.f, 0.f},
                PhysicsSimulationTier::Contact));
        }
        server.tick(dt);
        client.tick(dt);

        auto const* entry = client.snapshotAt(0);
        if (entry == nullptr || !entry->valid || entry->payloadLen == 0u) {
            continue;
        }
        sawSnapshot = true;
        if (entry->payloadLen > cfg.maxSnapshotBytesPerPeer) {
            return 5;
        }
        EntityKinematicsSnapshot snaps[32]{};
        std::size_t const n = client.readLatestEntities(snaps, 32);
        if (n == 0u) {
            return 6;
        }
        if (entry->payloadLen != n * kEntityKinematicsSnapshotWireBytes) {
            return 7;
        }
        if (entry->payloadLen > maxPayloadObserved) {
            maxPayloadObserved = entry->payloadLen;
        }
        if (n > maxEntitiesObserved) {
            maxEntitiesObserved = n;
        }
    }

    if (!sawSnapshot) {
        return 8;
    }
    if (maxPayloadObserved > cfg.maxSnapshotBytesPerPeer) {
        return 9;
    }
    if (maxEntitiesObserved * kEntityKinematicsSnapshotWireBytes > cfg.maxSnapshotBytesPerPeer) {
        return 10;
    }

    return 0;
}
