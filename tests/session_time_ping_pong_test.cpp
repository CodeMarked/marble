// Loopback integration: protocol v4 TimePing / TimePong after HelloAck (no crash; session stays up).

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"

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
    if (!client.initialize(&pair.b, kServerPeerId, 60u, 0xC001D00Du)) {
        return 2;
    }

    float const dt = 1.0f / 60.0f;

    client.tick(dt);
    server.tick(dt);
    client.tick(dt);

    if (client.state() != ConnectionState::Connected || !server.isConnected(kClientPeerId)) {
        return 3;
    }

    static_cast<void>(server.setEntity(0, {0x1000u}, {0.f, 1.f, 0.f}, {0.f, 0.f, 0.f},
                       PhysicsSimulationTier::Contact));

    // Enough paired steps for kTimePingIntervalTicks (30) plus delivery and optional retransmit.
    for (int i = 0; i < 100; ++i) {
        client.tick(dt);
        server.tick(dt);
    }

    if (client.state() != ConnectionState::Connected || !server.isConnected(kClientPeerId)) {
        return 4;
    }
    if (!client.hasServerTimeSync()) {
        return 5;
    }
    if (client.estimatedRttSeconds() <= 0.f) {
        return 6;
    }

    return 0;
}
