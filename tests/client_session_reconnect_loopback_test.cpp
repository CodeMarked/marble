// Loopback: disconnect then re-initialize ClientSession and handshake again; snapshots resume.

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"

#include <cstdint>

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

    if (!server.initialize(cfg, &pair.a)) {
        return 1;
    }

    ClientSession<32, 8> client{};
    if (!client.initialize(&pair.b, kServerPeerId, 60u, 0xC0DEC0DEu)) {
        return 2;
    }

    float const dt = 1.0f / 60.0f;

    client.tick(dt);
    server.tick(dt);
    client.tick(dt);
    if (client.state() != ConnectionState::Connected || !server.isConnected(kClientPeerId)) {
        return 3;
    }

    if (!server.setEntity(0, {0x1000u}, {1.f, 2.f, 3.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 4;
    }

    server.tick(dt);
    server.tick(dt);
    client.tick(dt);
    if (client.snapshotCount() == 0u) {
        return 5;
    }

    client.disconnect();
    server.tick(dt);
    // Do not `client.tick` here: a tick would transition Disconnected -> Connecting (Hello) before re-init.

    if (client.state() != ConnectionState::Disconnected) {
        return 6;
    }
    if (server.isConnected(kClientPeerId) || server.peerCount() != 0u) {
        return 7;
    }

    if (!client.initialize(&pair.b, kServerPeerId, 60u, 0x505EEDu)) {
        return 8;
    }

    client.tick(dt);
    server.tick(dt);
    client.tick(dt);
    if (client.state() != ConnectionState::Connected || !server.isConnected(kClientPeerId)) {
        return 9;
    }

    if (!server.setEntity(0, {0x1000u}, {2.f, 3.f, 4.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 10;
    }

    server.tick(dt);
    server.tick(dt);
    client.tick(dt);

    EntityKinematicsSnapshot snap{};
    if (client.readLatestEntities(&snap, 1) != 1u) {
        return 11;
    }
    if (snap.entity.guid != 0x1000u) {
        return 12;
    }

    return 0;
}
