#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"

#include <array>
#include <cstdint>

int main() {
    using namespace marble::gameplay;

    static constexpr PeerId kServerPeerId = 1u;
    static constexpr PeerId kClientPeerId = 2u;
    static constexpr std::size_t kMaxPayload = 2048;
    static constexpr std::size_t kQueueDepth = 64;

    LoopbackTransportPair<kMaxPayload, kQueueDepth> pair{kServerPeerId, kClientPeerId};

    // --- initialize authoritative session (DedicatedServer) ---
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
    if (server.currentTick() != 0u) {
        return 2;
    }

    // --- initialize client session ---
    ClientSession<32, 8> client{};
    if (!client.initialize(&pair.b, kServerPeerId, 60u, 0xBEEF1234u)) {
        return 3;
    }

    // --- client tick: sends Hello ---
    float const dt = 1.0f / 60.0f;
    client.tick(dt);
    if (client.state() != ConnectionState::Connecting) {
        return 4;
    }

    // --- server tick: receives Hello, sends reliable HelloAck ---
    server.tick(dt);
    if (server.currentTick() != 1u) {
        return 5;
    }

    // --- client tick: receives HelloAck, transitions to Connected ---
    client.tick(dt);
    if (client.state() != ConnectionState::Connected) {
        return 6;
    }
    if (client.assignedPeerId() != kClientPeerId) {
        return 7;
    }

    // --- verify server sees client as connected ---
    if (!server.isConnected(kClientPeerId)) {
        return 8;
    }
    if (server.peerCount() != 1u) {
        return 9;
    }

    // --- set entity state on server ---
    if (!server.setEntity(0, {0x1000u}, {1.f, 2.f, 3.f}, {4.f, 5.f, 6.f},
                          PhysicsSimulationTier::Contact)) {
        return 10;
    }

    // --- advance server until snapshot emission (every 3 ticks at 60/20) ---
    // Server already at tick 1; we need tick 3 for first snapshot.
    server.tick(dt);
    server.tick(dt);
    // Server is now at tick 3; snapshot should have been emitted.

    // --- client tick: receive snapshot ---
    client.tick(dt);
    if (client.snapshotCount() == 0u) {
        return 11;
    }

    // --- parse received entities ---
    EntityKinematicsSnapshot snaps[4]{};
    std::size_t const count = client.readLatestEntities(snaps, 4);
    if (count != 1u) {
        return 12;
    }
    if (snaps[0].entity.guid != 0x1000u) {
        return 13;
    }
    if (snaps[0].simTick != 3u) {
        return 14;
    }

    // --- server HelloAck retransmit buffer retired after client ack ---
    // Client sent an Ack when it received the reliable HelloAck.
    // Tick server to process it.
    server.tick(dt);
    auto const* ps = server.peerStateFor(kClientPeerId);
    if (ps == nullptr) {
        return 15;
    }
    if (ps->channel.activeCount() != 0u) {
        return 16; // HelloAck should be acked and retired
    }

    // --- run several more snapshot cycles to verify steady state ---
    for (int i = 0; i < 9; ++i) {
        server.tick(dt);
    }
    for (int i = 0; i < 3; ++i) {
        client.tick(dt);
    }
    if (client.snapshotCount() < 2u) {
        return 17;
    }

    // --- client disconnect ---
    client.disconnect();
    server.tick(dt);

    if (server.isConnected(kClientPeerId)) {
        return 18;
    }
    if (server.peerCount() != 0u) {
        return 19;
    }

    // --- duplicate Hello handling: reconnect same peer ---
    LoopbackTransportPair<kMaxPayload, kQueueDepth> pair2{kServerPeerId, kClientPeerId};
    AuthoritativeSession<8> server2{};
    if (!server2.initialize(cfg, &pair2.a)) {
        return 20;
    }

    ClientSession<32, 8> client2{};
    if (!client2.initialize(&pair2.b, kServerPeerId, 60u, 0xAAAAu)) {
        return 21;
    }

    client2.tick(dt);
    server2.tick(dt);
    // Simulate duplicate Hello: tick client again without reading HelloAck
    // (client resends after timeout, but for loopback we just re-tick)
    client2.tick(dt);
    if (client2.state() != ConnectionState::Connected) {
        return 22;
    }

    // --- reject invalid SessionConfig ---
    AuthoritativeSession<8> badServer{};
    SessionConfig badCfg{};
    badCfg.mode = MultiplayerMode::Offline;
    badCfg.maxPlayers = 1u;
    badCfg.simulationHz = 60u;
    badCfg.snapshotHz = 20u;
    if (badServer.initialize(badCfg, &pair2.a)) {
        return 23; // Offline not allowed for AuthoritativeSession
    }

    // --- null transport rejected ---
    if (badServer.initialize(cfg, nullptr)) {
        return 24;
    }

    return 0;
}
