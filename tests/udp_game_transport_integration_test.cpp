#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/UdpGameTransport.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace {

[[nodiscard]] bool snapshotsNearEqual(
    marble::gameplay::EntityKinematicsSnapshot const& a,
    marble::gameplay::EntityKinematicsSnapshot const& b
) noexcept {
    using marble::gameplay::PhysicsSimulationTier;
    if (a.simTick != b.simTick || a.entity.guid != b.entity.guid || a.tier != b.tier) {
        return false;
    }
    auto near3 = [](marble::math::Vec3 const& u, marble::math::Vec3 const& v) {
        return std::fabs(u.x - v.x) < 1e-5f && std::fabs(u.y - v.y) < 1e-5f && std::fabs(u.z - v.z) < 1e-5f;
    };
    return near3(a.positionLocal, b.positionLocal) && near3(a.linearVelocity, b.linearVelocity);
}

[[nodiscard]] bool roundTripOnce(
    marble::gameplay::UdpGameTransport& from,
    marble::gameplay::UdpGameTransport& to,
    marble::gameplay::PeerId fromId,
    marble::gameplay::PeerId toId,
    marble::gameplay::EntityKinematicsSnapshot const& snap
) {
    using namespace marble::gameplay;
    std::array<std::uint8_t, 256> wire{};
    std::size_t const n = writeEntityKinematicsSnapshot(wire.data(), wire.size(), snap);
    if (n != kEntityKinematicsSnapshotWireBytes) {
        return false;
    }
    if (!from.send(toId, wire.data(), n)) {
        return false;
    }
    constexpr int kMaxSpins = 100000;
    for (int spin = 0; spin < kMaxSpins; ++spin) {
        (void)spin;
        PeerId src{kInvalidPeerId};
        std::array<std::uint8_t, 256> rx{};
        std::size_t const nr = to.receive(src, rx.data(), rx.size());
        if (nr == 0u) {
            continue;
        }
        if (src != fromId || nr != n) {
            return false;
        }
        EntityKinematicsSnapshot got{};
        if (!readEntityKinematicsSnapshot(rx.data(), nr, got)) {
            return false;
        }
        return snapshotsNearEqual(snap, got);
    }
    return false;
}

} // namespace

int main() {
    using namespace marble::gameplay;

    UdpGameTransport host{};
    UdpGameTransport client{};

    if (!host.bind(0u) || !client.bind(0u)) {
        return 1;
    }

    PeerId const kHostId = 1u;
    PeerId const kClientId = 2u;

    std::uint16_t const hostPort = host.localPort();
    std::uint16_t const clientPort = client.localPort();
    if (hostPort == 0u || clientPort == 0u) {
        return 2;
    }

    if (!host.addPeer(kClientId, "127.0.0.1", clientPort)) {
        return 3;
    }
    if (!client.addPeer(kHostId, "127.0.0.1", hostPort)) {
        return 4;
    }

    EntityKinematicsSnapshot snap{};
    snap.simTick = 42u;
    snap.entity = {0xcafeull};
    snap.tier = PhysicsSimulationTier::CruiseOrbit;
    snap.positionLocal = {1.f, -2.f, 3.25f};
    snap.linearVelocity = {10.f, 20.f, 30.f};

    if (!roundTripOnce(host, client, kHostId, kClientId, snap)) {
        return 5;
    }

    snap.simTick = 99u;
    snap.linearVelocity = {-1.f, 0.f, 0.5f};
    if (!roundTripOnce(client, host, kClientId, kHostId, snap)) {
        return 6;
    }

    SimulationIsland island{};
    island.originX = 1e6;
    island.originY = 0.25;
    island.originZ = -3.5;

    std::array<std::uint8_t, 64> anchorWire{};
    std::size_t const na = writeSimulationIslandAnchor(anchorWire.data(), anchorWire.size(), island);
    if (na != kSimulationIslandAnchorWireBytes) {
        return 7;
    }
    if (!host.send(kClientId, anchorWire.data(), na)) {
        return 8;
    }
    for (int spin = 0; spin < 100000; ++spin) {
        (void)spin;
        PeerId src{kInvalidPeerId};
        std::array<std::uint8_t, 64> rx{};
        std::size_t const nr = client.receive(src, rx.data(), rx.size());
        if (nr == 0u) {
            continue;
        }
        if (src != kHostId || nr != na) {
            return 9;
        }
        SimulationIsland got{};
        if (!readSimulationIslandAnchor(rx.data(), nr, got)) {
            return 10;
        }
        if (got.originX != island.originX || got.originY != island.originY || got.originZ != island.originZ) {
            return 11;
        }
        return 0;
    }
    return 12;
}
