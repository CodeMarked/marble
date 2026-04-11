// Synthetic client + authoritative session metrics (snapshot rate, bytes, simTick lag, RTT, entity counts).
// Phase B: two-client loopback hub + per-peer AOI checks.

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
#include "impaired_game_transport.hpp"
#include "loopback_hub_transport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

using namespace marble::gameplay;
using marble::gameplay::test::ImpairedGameTransport;
using marble::gameplay::test::LoopbackHubClientTransport;
using marble::gameplay::test::LoopbackHubServerTransport;
using marble::gameplay::test::LoopbackHubShared;

[[nodiscard]] float percentileSorted(std::vector<float> const& sorted, float p01) noexcept {
    if (sorted.empty()) {
        return 0.f;
    }
    std::size_t const n = sorted.size();
    float const idx = p01 * static_cast<float>(n - 1u);
    std::size_t const lo = static_cast<std::size_t>(std::floor(idx));
    std::size_t const hi = static_cast<std::size_t>(std::ceil(idx));
    if (lo >= n) {
        return sorted.back();
    }
    if (hi >= n) {
        return sorted.back();
    }
    float const t = idx - static_cast<float>(lo);
    return sorted[lo] * (1.f - t) + sorted[hi] * t;
}

[[nodiscard]] int runHubAoiTwoClients() {
    static constexpr PeerId kServerPeerId = 1u;
    static constexpr PeerId kClientPeerId2 = 2u;
    static constexpr PeerId kClientPeerId3 = 3u;
    static constexpr std::size_t kMaxPayload = 2048;
    static constexpr std::size_t kQueueDepth = 64;

    LoopbackHubShared<kMaxPayload, kQueueDepth> hub{};
    LoopbackHubServerTransport<kMaxPayload, kQueueDepth> transportServer(&hub);
    LoopbackHubClientTransport<kMaxPayload, kQueueDepth> transport2(&hub, kClientPeerId2, &hub.toClient2);
    LoopbackHubClientTransport<kMaxPayload, kQueueDepth> transport3(&hub, kClientPeerId3, &hub.toClient3);

    AuthoritativeSession<8> server{};
    SessionConfig cfg{};
    cfg.mode = MultiplayerMode::DedicatedServer;
    cfg.maxPlayers = 8u;
    cfg.simulationHz = 60u;
    cfg.snapshotHz = 20u;
    cfg.maxPredictionTicks = 2u;

    if (!server.initialize(cfg, &transportServer)) {
        return 20;
    }

    ClientSession<32, 8> client2{};
    ClientSession<32, 8> client3{};
    if (!client2.initialize(&transport2, kServerPeerId, 60u, 0xB2u)) {
        return 21;
    }
    if (!client3.initialize(&transport3, kServerPeerId, 60u, 0xB3u)) {
        return 22;
    }

    float const dt = 1.0f / 60.0f;
    for (int i = 0; i < 40; ++i) {
        client2.tick(dt);
        server.tick(dt);
    }
    for (int i = 0; i < 40; ++i) {
        client3.tick(dt);
        server.tick(dt);
    }
    if (client2.state() != ConnectionState::Connected || client3.state() != ConnectionState::Connected) {
        return 23;
    }
    if (client2.assignedPeerId() != kClientPeerId2 || client3.assignedPeerId() != kClientPeerId3) {
        return 24;
    }

    if (!server.setEntity(0, {0x301u}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 25;
    }
    if (!server.setEntity(1, {0x302u}, {500.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 26;
    }
    if (!server.setEntity(2, {0x303u}, {2000.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 27;
    }
    if (!server.setEntity(3, {0x304u}, {2200.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 28;
    }

    server.setAoiEnabled(true);
    server.setDefaultAoiRadius(100.f);
    server.setPeerViewEntity(kClientPeerId2, 0u);
    server.setPeerViewEntity(kClientPeerId3, 2u);

    for (int i = 0; i < 30; ++i) {
        client2.tick(dt);
        client3.tick(dt);
        server.tick(dt);
    }

    EntityKinematicsSnapshot snaps2[8]{};
    EntityKinematicsSnapshot snaps3[8]{};
    std::size_t const n2 = client2.readLatestEntities(snaps2, 8);
    std::size_t const n3 = client3.readLatestEntities(snaps3, 8);
    if (n2 != 1u || snaps2[0].entity.guid != 0x301u) {
        return 29;
    }
    if (n3 != 1u || snaps3[0].entity.guid != 0x303u) {
        return 30;
    }

    std::printf("  multiplayer_metrics: hub_aoi two_clients ok (peer2_entities=%zu peer3_entities=%zu)\n", n2, n3);
    return 0;
}

} // namespace

int main() {
    using namespace marble::gameplay;
    using marble::gameplay::test::ImpairedGameTransport;

    static constexpr PeerId kServerPeerId = 1u;
    static constexpr PeerId kClientPeerId = 2u;
    static constexpr std::size_t kMaxPayload = 2048;
    static constexpr std::size_t kQueueDepth = 64;

    LoopbackTransportPair<kMaxPayload, kQueueDepth> pair{kServerPeerId, kClientPeerId};
    ImpairedGameTransport impairedClient(&pair.b, 0u, 0u);

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
    if (!client.initialize(&impairedClient, kServerPeerId, 60u, 0xBEEF1234u)) {
        return 2;
    }

    if (!server.setEntity(0, {0x1000u}, {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 3;
    }

    float const dt = 1.0f / 60.0f;
    std::uint32_t snapshotEvents = 0u;
    std::uint64_t snapshotBytesTotal = 0u;
    std::uint32_t tickLagMin = 0xFFFFFFFFu;
    std::uint32_t tickLagMax = 0u;
    double tickLagSum = 0.0;
    std::vector<float> rttSamples;
    std::vector<std::size_t> entityCountsPerSnap;
    rttSamples.reserve(400);
    entityCountsPerSnap.reserve(400);

    client.tick(dt);
    server.tick(dt);
    client.tick(dt);

    if (client.state() != ConnectionState::Connected) {
        return 4;
    }

    std::uint32_t prevSnapTick = 0u;
    bool hadPrevTick = false;

    for (int step = 0; step < 360; ++step) {
        client.tick(dt);
        server.tick(dt);

        if (client.state() == ConnectionState::Connected) {
            float const rtt = client.estimatedRttSeconds();
            if (rtt > 1e-6f) {
                rttSamples.push_back(rtt);
            }
        }

        if (client.snapshotCount() == 0u) {
            continue;
        }
        auto const* entry = client.snapshotAt(0);
        if (entry == nullptr || !entry->valid || entry->payloadLen < kEntityKinematicsSnapshotWireBytes) {
            continue;
        }
        if (entry->payloadLen % kEntityKinematicsSnapshotWireBytes != 0u) {
            return 5;
        }
        entityCountsPerSnap.push_back(entry->payloadLen / kEntityKinematicsSnapshotWireBytes);

        EntityKinematicsSnapshot probe{};
        if (!readEntityKinematicsSnapshot(entry->payload.data(), entry->payloadLen, probe)) {
            continue;
        }
        if (!hadPrevTick || probe.simTick != prevSnapTick) {
            hadPrevTick = true;
            prevSnapTick = probe.simTick;
            ++snapshotEvents;
            snapshotBytesTotal += entry->payloadLen;

            std::uint32_t const srvTick = server.currentTick();
            std::uint32_t const lag = (srvTick >= probe.simTick) ? (srvTick - probe.simTick) : 0u;
            tickLagMin = std::min(tickLagMin, lag);
            tickLagMax = std::max(tickLagMax, lag);
            tickLagSum += static_cast<double>(lag);
        }
    }

    if (snapshotEvents < 10u) {
        std::fprintf(stderr, "metrics: expected >=10 snapshot events, got %u\n", snapshotEvents);
        return 6;
    }

    double const avgLag = tickLagSum / static_cast<double>(snapshotEvents);
    double const avgBytes = static_cast<double>(snapshotBytesTotal) / static_cast<double>(snapshotEvents);
    std::printf(
        "  multiplayer_metrics: snapshots=%u total_bytes=%llu avg_bytes_per_snap=%.1f "
        "simTick_lag min=%u max=%u avg=%.2f drops=%u\n",
        snapshotEvents,
        static_cast<unsigned long long>(snapshotBytesTotal),
        avgBytes,
        tickLagMin,
        tickLagMax,
        avgLag,
        static_cast<unsigned>(impairedClient.dropsObserved()));

    if (avgLag > 25.0) {
        std::fprintf(stderr, "metrics: avg simTick lag too high: %.2f\n", avgLag);
        return 7;
    }

    if (!entityCountsPerSnap.empty()) {
        std::size_t sumEnt = 0u;
        std::size_t maxEnt = 0u;
        for (std::size_t c : entityCountsPerSnap) {
            sumEnt += c;
            maxEnt = std::max(maxEnt, c);
        }
        double const avgEnt =
            static_cast<double>(sumEnt) / static_cast<double>(entityCountsPerSnap.size());
        std::printf(
            "  multiplayer_metrics: entities_per_snap avg=%.2f max=%zu samples=%zu\n",
            avgEnt,
            maxEnt,
            entityCountsPerSnap.size());
    }

    if (!rttSamples.empty()) {
        std::sort(rttSamples.begin(), rttSamples.end());
        float const rMin = rttSamples.front();
        float const rMax = rttSamples.back();
        double rSum = 0.0;
        for (float v : rttSamples) {
            rSum += static_cast<double>(v);
        }
        double const rMean = rSum / static_cast<double>(rttSamples.size());
        float const p50 = percentileSorted(rttSamples, 0.50f);
        float const p95 = percentileSorted(rttSamples, 0.95f);
        std::printf(
            "  multiplayer_metrics: rtt_s samples=%zu min=%.6f max=%.6f mean=%.6f p50=%.6f p95=%.6f\n",
            rttSamples.size(),
            static_cast<double>(rMin),
            static_cast<double>(rMax),
            rMean,
            static_cast<double>(p50),
            static_cast<double>(p95));
    } else {
        std::printf("  multiplayer_metrics: rtt_s samples=0 (no RTT samples collected)\n");
    }

    // Impaired path: separate loopback fabric so queues are clean.
    LoopbackTransportPair<kMaxPayload, kQueueDepth> pair2{kServerPeerId, kClientPeerId};
    ImpairedGameTransport impaired2(&pair2.b, 4u, 0x12345678u);
    AuthoritativeSession<8> server2{};
    if (!server2.initialize(cfg, &pair2.a)) {
        return 8;
    }
    ClientSession<32, 8> client2{};
    if (!client2.initialize(&impaired2, kServerPeerId, 60u, 0xCAFEu)) {
        return 9;
    }
    if (!server2.setEntity(0, {0x1000u}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact)) {
        return 10;
    }
    client2.tick(dt);
    server2.tick(dt);
    client2.tick(dt);
    if (client2.state() != ConnectionState::Connected) {
        return 11;
    }
    for (int i = 0; i < 600; ++i) {
        client2.tick(dt);
        server2.tick(dt);
    }
    if (client2.snapshotCount() == 0u) {
        std::fprintf(stderr, "metrics: impaired transport produced no snapshots\n");
        return 12;
    }
    std::printf("  multiplayer_metrics: impaired path drops_observed=%u\n",
        static_cast<unsigned>(impaired2.dropsObserved()));

    int const hubRc = runHubAoiTwoClients();
    if (hubRc != 0) {
        return hubRc;
    }

    return 0;
}
