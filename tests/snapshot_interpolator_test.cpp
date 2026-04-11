#include "gameplay/SnapshotInterpolator.hpp"
#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"

#include <array>
#include <cmath>
#include <cstdint>

static bool approxEq(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

static bool vec3Eq(marble::math::Vec3 a, marble::math::Vec3 b, float eps = 1e-4f) {
    return approxEq(a.x, b.x, eps) && approxEq(a.y, b.y, eps) && approxEq(a.z, b.z, eps);
}

int main() {
    using namespace marble::gameplay;

    static constexpr WorldObjectRef kEntA{0x1000u};
    static constexpr WorldObjectRef kEntB{0x2000u};

    // --- empty timeline returns 0 ---
    {
        SnapshotInterpolator<4, 8> interp{};
        InterpolatedEntity out[4]{};
        if (interp.interpolate(5u, out, 4u) != 0u) {
            return 1;
        }
        if (interp.frameCount() != 0u) {
            return 2;
        }
        if (interp.latestTick() != 0u) {
            return 3;
        }
    }

    // --- single frame: returns entities with extrapolated = true ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot snap{};
        snap.simTick = 10u;
        snap.entity = kEntA;
        snap.positionLocal = {1.f, 2.f, 3.f};
        snap.linearVelocity = {0.f, 0.f, 0.f};
        interp.pushSnapshot(10u, &snap, 1u);

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(10u, out, 4u);
        if (n != 1u) {
            return 10;
        }
        if (out[0].entity != kEntA) {
            return 11;
        }
        if (!vec3Eq(out[0].position, {1.f, 2.f, 3.f})) {
            return 12;
        }
        if (!out[0].extrapolated) {
            return 13;
        }
    }

    // --- two-frame interpolation: midpoint ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot s1{};
        s1.simTick = 10u;
        s1.entity = kEntA;
        s1.positionLocal = {0.f, 0.f, 0.f};
        s1.linearVelocity = {1.f, 0.f, 0.f};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 16u;
        s2.entity = kEntA;
        s2.positionLocal = {6.f, 0.f, 0.f};
        s2.linearVelocity = {1.f, 0.f, 0.f};

        interp.pushSnapshot(10u, &s1, 1u);
        interp.pushSnapshot(16u, &s2, 1u);

        // renderTick = 13 => t = (13-10)/(16-10) = 0.5
        InterpolatedEntity out[4]{};
        std::size_t n = interp.interpolate(13u, out, 4u);
        if (n != 1u) {
            return 20;
        }
        if (!vec3Eq(out[0].position, {3.f, 0.f, 0.f})) {
            return 21;
        }
        if (out[0].extrapolated) {
            return 22;
        }

        // renderTick = 12 => t = 2/6 = 1/3
        n = interp.interpolate(12u, out, 4u);
        if (n != 1u) {
            return 23;
        }
        if (!approxEq(out[0].position.x, 2.f)) {
            return 24;
        }

        // Fractional render tick: 12.5 => t = 2.5/6, pos.x = 2.5
        n = interp.interpolate(12.5f, out, 4u);
        if (n != 1u) {
            return 25;
        }
        if (!approxEq(out[0].position.x, 2.5f)) {
            return 26;
        }
    }

    // --- exact tick match on older frame (t = 0) ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot s1{};
        s1.simTick = 10u;
        s1.entity = kEntA;
        s1.positionLocal = {1.f, 2.f, 3.f};
        s1.linearVelocity = {0.f, 0.f, 0.f};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 20u;
        s2.entity = kEntA;
        s2.positionLocal = {11.f, 12.f, 13.f};
        s2.linearVelocity = {0.f, 0.f, 0.f};

        interp.pushSnapshot(10u, &s1, 1u);
        interp.pushSnapshot(20u, &s2, 1u);

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(10u, out, 4u);
        if (n != 1u) {
            return 30;
        }
        if (!vec3Eq(out[0].position, {1.f, 2.f, 3.f})) {
            return 31;
        }
        if (out[0].extrapolated) {
            return 32;
        }
    }

    // --- exact tick match on newer frame (t = 1) ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot s1{};
        s1.simTick = 10u;
        s1.entity = kEntA;
        s1.positionLocal = {0.f, 0.f, 0.f};
        s1.linearVelocity = {};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 20u;
        s2.entity = kEntA;
        s2.positionLocal = {10.f, 10.f, 10.f};
        s2.linearVelocity = {};

        interp.pushSnapshot(10u, &s1, 1u);
        interp.pushSnapshot(20u, &s2, 1u);

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(20u, out, 4u);
        if (n != 1u) {
            return 35;
        }
        if (!vec3Eq(out[0].position, {10.f, 10.f, 10.f})) {
            return 36;
        }
        if (out[0].extrapolated) {
            return 37;
        }
    }

    // --- extrapolation past newest frame ---
    {
        SnapshotInterpolator<4, 8> interp{};
        interp.setMaxExtrapolationTicks(10u);

        EntityKinematicsSnapshot s1{};
        s1.simTick = 10u;
        s1.entity = kEntA;
        s1.positionLocal = {0.f, 0.f, 0.f};
        s1.linearVelocity = {2.f, 0.f, 0.f};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 13u;
        s2.entity = kEntA;
        s2.positionLocal = {6.f, 0.f, 0.f};
        s2.linearVelocity = {2.f, 0.f, 0.f};

        interp.pushSnapshot(10u, &s1, 1u);
        interp.pushSnapshot(13u, &s2, 1u);

        // renderTick = 15, delta = 2 ticks past frame 13
        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(15u, out, 4u);
        if (n != 1u) {
            return 40;
        }
        // position = 6 + 2 * 2 = 10
        if (!approxEq(out[0].position.x, 10.f)) {
            return 41;
        }
        if (!out[0].extrapolated) {
            return 42;
        }
        // velocity preserved
        if (!approxEq(out[0].velocity.x, 2.f)) {
            return 43;
        }
    }

    // --- extrapolation clamped to maxExtrapolationTicks ---
    {
        SnapshotInterpolator<4, 8> interp{};
        interp.setMaxExtrapolationTicks(3u);

        EntityKinematicsSnapshot snap{};
        snap.simTick = 10u;
        snap.entity = kEntA;
        snap.positionLocal = {0.f, 0.f, 0.f};
        snap.linearVelocity = {1.f, 0.f, 0.f};

        interp.pushSnapshot(10u, &snap, 1u);
        interp.pushSnapshot(10u, &snap, 1u); // need 2 frames for bracket logic

        // renderTick = 20, delta = 10, but clamped to 3
        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(20u, out, 4u);
        if (n != 1u) {
            return 50;
        }
        // position = 0 + 1 * 3 = 3 (not 10)
        if (!approxEq(out[0].position.x, 3.f)) {
            return 51;
        }
        if (!out[0].extrapolated) {
            return 52;
        }
    }

    // --- render tick before oldest frame ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot snap{};
        snap.simTick = 10u;
        snap.entity = kEntA;
        snap.positionLocal = {5.f, 5.f, 5.f};
        snap.linearVelocity = {};

        interp.pushSnapshot(10u, &snap, 1u);
        snap.simTick = 20u;
        interp.pushSnapshot(20u, &snap, 1u);

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(5u, out, 4u);
        if (n != 1u) {
            return 60;
        }
        if (!vec3Eq(out[0].position, {5.f, 5.f, 5.f})) {
            return 61;
        }
        if (!out[0].extrapolated) {
            return 62;
        }
    }

    // --- entity appears in newer frame only ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot s1{};
        s1.simTick = 10u;
        s1.entity = kEntA;
        s1.positionLocal = {1.f, 0.f, 0.f};
        s1.linearVelocity = {};

        std::array<EntityKinematicsSnapshot, 2> s2arr{};
        s2arr[0].simTick = 20u;
        s2arr[0].entity = kEntA;
        s2arr[0].positionLocal = {2.f, 0.f, 0.f};
        s2arr[0].linearVelocity = {};
        s2arr[1].simTick = 20u;
        s2arr[1].entity = kEntB;
        s2arr[1].positionLocal = {100.f, 0.f, 0.f};
        s2arr[1].linearVelocity = {};

        interp.pushSnapshot(10u, &s1, 1u);
        interp.pushSnapshot(20u, s2arr.data(), 2u);

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(15u, out, 4u);
        if (n != 2u) {
            return 70;
        }

        // kEntA: interpolated (present in both)
        bool foundA = false, foundB = false;
        for (std::size_t i = 0u; i < n; ++i) {
            if (out[i].entity == kEntA) {
                foundA = true;
                if (out[i].extrapolated) {
                    return 71;
                }
                if (!approxEq(out[i].position.x, 1.5f)) {
                    return 72;
                }
            } else if (out[i].entity == kEntB) {
                foundB = true;
                if (!out[i].extrapolated) {
                    return 73;
                }
            }
        }
        if (!foundA || !foundB) {
            return 74;
        }
    }

    // --- entity disappears in newer frame ---
    {
        SnapshotInterpolator<4, 8> interp{};
        std::array<EntityKinematicsSnapshot, 2> s1arr{};
        s1arr[0].simTick = 10u;
        s1arr[0].entity = kEntA;
        s1arr[0].positionLocal = {1.f, 0.f, 0.f};
        s1arr[0].linearVelocity = {};
        s1arr[1].simTick = 10u;
        s1arr[1].entity = kEntB;
        s1arr[1].positionLocal = {50.f, 0.f, 0.f};
        s1arr[1].linearVelocity = {};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 20u;
        s2.entity = kEntA;
        s2.positionLocal = {2.f, 0.f, 0.f};
        s2.linearVelocity = {};

        interp.pushSnapshot(10u, s1arr.data(), 2u);
        interp.pushSnapshot(20u, &s2, 1u);

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(15u, out, 4u);
        if (n != 2u) {
            return 80;
        }

        bool bExtrapolated = false;
        for (std::size_t i = 0u; i < n; ++i) {
            if (out[i].entity == kEntB) {
                bExtrapolated = out[i].extrapolated;
            }
        }
        if (!bExtrapolated) {
            return 81;
        }
    }

    // --- ring overflow: oldest evicted, interpolation uses surviving frames ---
    {
        SnapshotInterpolator<4, 4> interp{};
        EntityKinematicsSnapshot snap{};
        snap.entity = kEntA;
        snap.linearVelocity = {};

        for (std::uint32_t t = 0u; t < 6u; ++t) {
            snap.simTick = t * 3u;
            snap.positionLocal = {static_cast<float>(t * 3u), 0.f, 0.f};
            interp.pushSnapshot(snap.simTick, &snap, 1u);
        }
        // Pushed ticks: 0, 3, 6, 9, 12, 15. Capacity = 4, so oldest 2 evicted.
        // Surviving: 6, 9, 12, 15.
        if (interp.frameCount() != 4u) {
            return 90;
        }
        if (interp.latestTick() != 15u) {
            return 91;
        }

        // Interpolate at tick 10 (between frames 9 and 12), t = 1/3
        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(10u, out, 4u);
        if (n != 1u) {
            return 92;
        }
        // position = lerp(9, 12, 1/3) = 10
        if (!approxEq(out[0].position.x, 10.f)) {
            return 93;
        }
        if (out[0].extrapolated) {
            return 94;
        }

        // Tick 3 was evicted — falls before oldest surviving (6) => oldest with extrapolated
        std::size_t const n2 = interp.interpolate(3u, out, 4u);
        if (n2 != 1u) {
            return 95;
        }
        if (!approxEq(out[0].position.x, 6.f)) {
            return 96;
        }
        if (!out[0].extrapolated) {
            return 97;
        }
    }

    // --- suggestRenderTick ---
    {
        SnapshotInterpolator<4, 8> interp{};
        interp.setRenderDelayTicks(6u);

        EntityKinematicsSnapshot snap{};
        snap.entity = kEntA;
        snap.simTick = 20u;
        snap.positionLocal = {};
        snap.linearVelocity = {};
        interp.pushSnapshot(20u, &snap, 1u);

        if (interp.suggestRenderTick() != 14u) {
            return 100;
        }

        // When latest < delay, clamp to 0
        SnapshotInterpolator<4, 8> interp2{};
        interp2.setRenderDelayTicks(100u);
        interp2.pushSnapshot(5u, &snap, 1u);
        if (interp2.suggestRenderTick() != 0u) {
            return 101;
        }
    }

    // --- velocity interpolation ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot s1{};
        s1.simTick = 0u;
        s1.entity = kEntA;
        s1.positionLocal = {};
        s1.linearVelocity = {0.f, 0.f, 0.f};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 10u;
        s2.entity = kEntA;
        s2.positionLocal = {};
        s2.linearVelocity = {10.f, 0.f, 0.f};

        interp.pushSnapshot(0u, &s1, 1u);
        interp.pushSnapshot(10u, &s2, 1u);

        InterpolatedEntity out[4]{};
        static_cast<void>(interp.interpolate(5u, out, 4u));
        // velocity = lerp(0, 10, 0.5) = 5
        if (!approxEq(out[0].velocity.x, 5.f)) {
            return 110;
        }
    }

    // --- tier selection during interpolation ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot s1{};
        s1.simTick = 0u;
        s1.entity = kEntA;
        s1.tier = PhysicsSimulationTier::Contact;
        s1.positionLocal = {};
        s1.linearVelocity = {};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 10u;
        s2.entity = kEntA;
        s2.tier = PhysicsSimulationTier::CruiseOrbit;
        s2.positionLocal = {};
        s2.linearVelocity = {};

        interp.pushSnapshot(0u, &s1, 1u);
        interp.pushSnapshot(10u, &s2, 1u);

        InterpolatedEntity out[4]{};
        // t = 3/10 = 0.3 < 0.5 => older tier (Contact)
        static_cast<void>(interp.interpolate(3u, out, 4u));
        if (out[0].tier != PhysicsSimulationTier::Contact) {
            return 120;
        }

        // t = 7/10 = 0.7 >= 0.5 => newer tier (CruiseOrbit)
        static_cast<void>(interp.interpolate(7u, out, 4u));
        if (out[0].tier != PhysicsSimulationTier::CruiseOrbit) {
            return 121;
        }
    }

    // --- reset clears timeline ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot snap{};
        snap.entity = kEntA;
        snap.simTick = 10u;
        snap.positionLocal = {};
        snap.linearVelocity = {};
        interp.pushSnapshot(10u, &snap, 1u);
        if (interp.frameCount() != 1u) {
            return 130;
        }
        interp.reset();
        if (interp.frameCount() != 0u) {
            return 131;
        }
        InterpolatedEntity out[4]{};
        if (interp.interpolate(10u, out, 4u) != 0u) {
            return 132;
        }
    }

    // --- null / zero-count edge cases ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot snap{};
        snap.entity = kEntA;
        snap.simTick = 10u;
        interp.pushSnapshot(10u, &snap, 1u);
        interp.pushSnapshot(20u, &snap, 1u);

        if (interp.interpolate(15u, nullptr, 4u) != 0u) {
            return 140;
        }
        InterpolatedEntity out[4]{};
        if (interp.interpolate(15u, out, 0u) != 0u) {
            return 141;
        }
    }

    // --- multiple entities interpolated together ---
    {
        SnapshotInterpolator<4, 8> interp{};

        std::array<EntityKinematicsSnapshot, 2> f1{};
        f1[0] = {10u, kEntA, PhysicsSimulationTier::Contact, {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}};
        f1[1] = {10u, kEntB, PhysicsSimulationTier::Contact, {0.f, 10.f, 0.f}, {0.f, -1.f, 0.f}};

        std::array<EntityKinematicsSnapshot, 2> f2{};
        f2[0] = {20u, kEntA, PhysicsSimulationTier::Contact, {10.f, 0.f, 0.f}, {1.f, 0.f, 0.f}};
        f2[1] = {20u, kEntB, PhysicsSimulationTier::Contact, {0.f, 0.f, 0.f}, {0.f, -1.f, 0.f}};

        interp.pushSnapshot(10u, f1.data(), 2u);
        interp.pushSnapshot(20u, f2.data(), 2u);

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(15u, out, 4u);
        if (n != 2u) {
            return 150;
        }

        for (std::size_t i = 0u; i < n; ++i) {
            if (out[i].entity == kEntA) {
                if (!approxEq(out[i].position.x, 5.f)) {
                    return 151;
                }
            } else if (out[i].entity == kEntB) {
                if (!approxEq(out[i].position.y, 5.f)) {
                    return 152;
                }
            }
        }
    }

    // --- integration with ClientSession: decode ring into interpolator ---
    {
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
            return 200;
        }

        ClientSession<32, 8> client{};
        if (!client.initialize(&pair.b, kServerPeerId, 60u, 0xFACEu)) {
            return 201;
        }

        float const dt = 1.0f / 60.0f;

        // Handshake
        client.tick(dt);
        server.tick(dt);
        client.tick(dt);
        if (client.state() != ConnectionState::Connected) {
            return 202;
        }

        // Set two entities on the server
        static_cast<void>(server.setEntity(0, kEntA, {10.f, 0.f, 0.f}, {1.f, 0.f, 0.f}));
        static_cast<void>(server.setEntity(1, kEntB, {0.f, 20.f, 0.f}, {0.f, 2.f, 0.f}));

        // Advance server to emit first snapshot (60/20 = every 3 ticks, server at tick 1)
        server.tick(dt);
        server.tick(dt);
        // Server at tick 3, first snapshot emitted.

        // Client receives snapshot
        client.tick(dt);
        if (client.snapshotCount() < 1u) {
            return 203;
        }

        // Decode into interpolator
        SnapshotInterpolator<4, 8> interp{};
        interp.setRenderDelayTicks(0u);

        auto const* entry = client.snapshotAt(0);
        if (entry == nullptr || !entry->valid) {
            return 204;
        }
        EntityKinematicsSnapshot decoded[4]{};
        std::size_t offset = 0u;
        std::size_t entityCount = 0u;
        while (offset + kEntityKinematicsSnapshotWireBytes <= entry->payloadLen && entityCount < 4u) {
            if (!readEntityKinematicsSnapshot(entry->payload.data() + offset,
                                              entry->payloadLen - offset, decoded[entityCount])) {
                break;
            }
            offset += kEntityKinematicsSnapshotWireBytes;
            ++entityCount;
        }
        if (entityCount != 2u) {
            return 205;
        }

        interp.pushSnapshot(decoded[0].simTick, decoded, entityCount);

        // Update server entities to new positions
        static_cast<void>(server.setEntity(0, kEntA, {13.f, 0.f, 0.f}, {1.f, 0.f, 0.f}));
        static_cast<void>(server.setEntity(1, kEntB, {0.f, 26.f, 0.f}, {0.f, 2.f, 0.f}));

        // Advance to next snapshot (3 more ticks)
        for (int i = 0; i < 3; ++i) {
            server.tick(dt);
        }
        client.tick(dt);

        if (client.snapshotCount() < 2u) {
            return 206;
        }

        auto const* entry2 = client.snapshotAt(0);
        if (entry2 == nullptr || !entry2->valid) {
            return 207;
        }
        offset = 0u;
        entityCount = 0u;
        while (offset + kEntityKinematicsSnapshotWireBytes <= entry2->payloadLen && entityCount < 4u) {
            if (!readEntityKinematicsSnapshot(entry2->payload.data() + offset,
                                              entry2->payloadLen - offset, decoded[entityCount])) {
                break;
            }
            offset += kEntityKinematicsSnapshotWireBytes;
            ++entityCount;
        }
        if (entityCount != 2u) {
            return 208;
        }

        interp.pushSnapshot(decoded[0].simTick, decoded, entityCount);

        // Now interpolate between the two server snapshots (ticks 3 and 6)
        std::uint32_t const renderTick = 4u;

        if (interp.latestTick() != 6u) {
            return 209;
        }

        InterpolatedEntity out[4]{};
        std::size_t const n = interp.interpolate(renderTick, out, 4u);
        if (n != 2u) {
            return 210;
        }

        // t = (4-3)/(6-3) = 1/3
        for (std::size_t i = 0u; i < n; ++i) {
            if (out[i].entity == kEntA) {
                // lerp(10, 13, 1/3) = 11
                if (!approxEq(out[i].position.x, 11.f)) {
                    return 211;
                }
                if (out[i].extrapolated) {
                    return 212;
                }
            } else if (out[i].entity == kEntB) {
                // lerp(20, 26, 1/3) = 22
                if (!approxEq(out[i].position.y, 22.f)) {
                    return 213;
                }
            }
        }
    }

    // --- velocity jump blend: large Δv uses newer velocity, position still lerped ---
    {
        SnapshotInterpolator<4, 8> interp{};
        interp.setVelocityJumpBlendThresholdMps(8.f);

        EntityKinematicsSnapshot s1{};
        s1.simTick = 0u;
        s1.entity = kEntA;
        s1.positionLocal = {0.f, 0.f, 0.f};
        s1.linearVelocity = {0.f, 0.f, 0.f};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 10u;
        s2.entity = kEntA;
        s2.positionLocal = {10.f, 0.f, 0.f};
        s2.linearVelocity = {10.f, 0.f, 0.f};

        interp.pushSnapshot(0u, &s1, 1u);
        interp.pushSnapshot(10u, &s2, 1u);

        InterpolatedEntity out[4]{};
        static_cast<void>(interp.interpolate(5u, out, 4u));
        if (!approxEq(out[0].position.x, 5.f)) {
            return 160;
        }
        if (!approxEq(out[0].velocity.x, 10.f)) {
            return 161;
        }
        if (out[0].extrapolated) {
            return 162;
        }

        interp.setVelocityJumpBlendThresholdMps(0.f);
        static_cast<void>(interp.interpolate(5u, out, 4u));
        if (!approxEq(out[0].velocity.x, 5.f)) {
            return 163;
        }
    }

    // --- tryVelocityDeltaBetweenLastTwoFrames ---
    {
        SnapshotInterpolator<4, 8> interp{};
        EntityKinematicsSnapshot s1{};
        s1.simTick = 0u;
        s1.entity = kEntA;
        s1.linearVelocity = {3.f, 4.f, 0.f};
        EntityKinematicsSnapshot s2{};
        s2.simTick = 10u;
        s2.entity = kEntA;
        s2.linearVelocity = {0.f, 0.f, 0.f};
        interp.pushSnapshot(0u, &s1, 1u);
        interp.pushSnapshot(10u, &s2, 1u);
        float d{};
        if (!interp.tryVelocityDeltaBetweenLastTwoFrames(kEntA, d)) {
            return 164;
        }
        if (!approxEq(d, 5.f)) {
            return 165;
        }
    }

    // --- velocity jump extrapolation: large Δv vs previous frame => no forward integration ---
    {
        SnapshotInterpolator<4, 8> interp{};
        interp.setMaxExtrapolationTicks(6u);
        interp.setVelocityJumpExtrapolationThresholdMps(5.f);

        EntityKinematicsSnapshot s1{};
        s1.simTick = 0u;
        s1.entity = kEntA;
        s1.positionLocal = {0.f, 0.f, 0.f};
        s1.linearVelocity = {0.f, 0.f, 0.f};

        EntityKinematicsSnapshot s2{};
        s2.simTick = 10u;
        s2.entity = kEntA;
        s2.positionLocal = {5.f, 0.f, 0.f};
        s2.linearVelocity = {10.f, 0.f, 0.f};

        interp.pushSnapshot(0u, &s1, 1u);
        interp.pushSnapshot(10u, &s2, 1u);

        InterpolatedEntity out[4]{};
        static_cast<void>(interp.interpolate(16u, out, 4u));
        if (!vec3Eq(out[0].position, {5.f, 0.f, 0.f})) {
            return 170;
        }
        if (!approxEq(out[0].velocity.x, 10.f)) {
            return 171;
        }

        interp.setVelocityJumpExtrapolationThresholdMps(0.f);
        static_cast<void>(interp.interpolate(16u, out, 4u));
        // dt = min(renderTick - newestSimTick, maxExtrap) = 6; x = 5 + 10*6
        if (!approxEq(out[0].position.x, 65.f)) {
            return 172;
        }
    }

    return 0;
}
