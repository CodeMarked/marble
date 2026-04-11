#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"

// End-to-end: AOI-enabled authoritative session sends only entities near the peer's view entity.

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
    if (!client.initialize(&pair.b, kServerPeerId, 60u, 0xC0FFEEu)) {
        return 2;
    }

    float const dt = 1.0f / 60.0f;

    client.tick(dt);
    server.tick(dt);
    client.tick(dt);
    if (client.state() != ConnectionState::Connected) {
        return 3;
    }
    if (!server.isConnected(kClientPeerId)) {
        return 4;
    }

    // View entity at origin; second entity far outside AOI radius.
    if (!server.setEntity(0, {0xA001u}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f},
                          PhysicsSimulationTier::Contact)) {
        return 5;
    }
    if (!server.setEntity(1, {0xA002u}, {500.f, 0.f, 0.f}, {0.f, 0.f, 0.f},
                          PhysicsSimulationTier::Contact)) {
        return 6;
    }

    server.setAoiEnabled(true);
    server.setDefaultAoiRadius(100.f);
    server.setPeerViewEntity(kClientPeerId, 0u);

    // Server at tick 1; need two more ticks for snapshot at tick 3 (60/20).
    server.tick(dt);
    server.tick(dt);
    client.tick(dt);

    EntityKinematicsSnapshot snaps[4]{};
    std::size_t const count = client.readLatestEntities(snaps, 4);
    if (count != 1u) {
        return 7;
    }
    if (snaps[0].entity.guid != 0xA001u) {
        return 8;
    }

    // Disable AOI: both entities should appear in the next snapshot cycle.
    server.setAoiEnabled(false);
    for (int i = 0; i < 3; ++i) {
        server.tick(dt);
    }
    client.tick(dt);

    std::size_t const count2 = client.readLatestEntities(snaps, 4);
    if (count2 != 2u) {
        return 9;
    }

    bool sawA001 = false;
    bool sawA002 = false;
    for (std::size_t i = 0; i < count2; ++i) {
        if (snaps[i].entity.guid == 0xA001u) {
            sawA001 = true;
        }
        if (snaps[i].entity.guid == 0xA002u) {
            sawA002 = true;
        }
    }
    if (!sawA001 || !sawA002) {
        return 10;
    }

    // Entity velocity lookahead: fast mover just outside static radius is excluded when lookahead=0,
    // included when lookahead expands effective radius (session path, not filter helper alone).
    {
        LoopbackTransportPair<kMaxPayload, kQueueDepth> pairB{kServerPeerId, kClientPeerId};
        AuthoritativeSession<8> srv{};
        if (!srv.initialize(cfg, &pairB.a)) {
            return 11;
        }
        ClientSession<32, 8> cli{};
        if (!cli.initialize(&pairB.b, kServerPeerId, 60u, 0xD00Du)) {
            return 12;
        }
        cli.tick(dt);
        srv.tick(dt);
        cli.tick(dt);
        if (cli.state() != ConnectionState::Connected) {
            return 13;
        }
        if (!srv.setEntity(0, {0xB001u}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
            return 14;
        }
        if (!srv.setEntity(1, {0xB002u}, {150.f, 0.f, 0.f}, {200.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
            return 15;
        }
        srv.setAoiEnabled(true);
        srv.setDefaultAoiRadius(100.f);
        srv.setAoiEntityVelocityLookaheadSeconds(0.f);
        srv.setPeerViewEntity(kClientPeerId, 0u);
        srv.tick(dt);
        srv.tick(dt);
        cli.tick(dt);
        EntityKinematicsSnapshot buf[4]{};
        std::size_t n = cli.readLatestEntities(buf, 4);
        if (n != 1u || buf[0].entity.guid != 0xB001u) {
            return 16;
        }
        srv.setAoiEntityVelocityLookaheadSeconds(0.5f);
        for (int i = 0; i < 3; ++i) {
            srv.tick(dt);
        }
        cli.tick(dt);
        n = cli.readLatestEntities(buf, 4);
        if (n != 2u) {
            return 17;
        }
        bool b1 = false;
        bool b2 = false;
        for (std::size_t i = 0u; i < n; ++i) {
            if (buf[i].entity.guid == 0xB001u) {
                b1 = true;
            }
            if (buf[i].entity.guid == 0xB002u) {
                b2 = true;
            }
        }
        if (!b1 || !b2) {
            return 18;
        }
    }

    return 0;
}
