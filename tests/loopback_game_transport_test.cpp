#include "gameplay/GameTransport.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"

#include <array>
#include <cstring>

int main() {
    using namespace marble::gameplay;

    LoopbackTransportPair<kEntityKinematicsSnapshotWireBytes, 4> pair(1u, 2u);
    IGameTransport& host = pair.a;
    IGameTransport& client = pair.b;

    EntityKinematicsSnapshot snap{};
    snap.simTick = 3u;
    snap.entity = {77u};
    snap.tier = PhysicsSimulationTier::Contact;
    snap.positionLocal = {1.f, 2.f, 3.f};
    snap.linearVelocity = {4.f, 5.f, 6.f};

    std::array<std::uint8_t, kEntityKinematicsSnapshotWireBytes> wire{};
    const std::size_t nw = writeEntityKinematicsSnapshot(wire.data(), wire.size(), snap);
    if (nw != kEntityKinematicsSnapshotWireBytes) {
        return 1;
    }

    PeerId wrong{99u};
    if (host.send(wrong, wire.data(), nw)) {
        return 2;
    }
    if (!host.send(2u, wire.data(), nw)) {
        return 3;
    }

    PeerId from{kInvalidPeerId};
    std::array<std::uint8_t, 256> rx{};
    const std::size_t nr = client.receive(from, rx.data(), 1u);
    if (nr != 0u) {
        return 4;
    }
    const std::size_t nr2 = client.receive(from, rx.data(), rx.size());
    if (nr2 != nw || from != 1u) {
        return 5;
    }
    if (std::memcmp(rx.data(), wire.data(), nw) != 0) {
        return 6;
    }

    EntityKinematicsSnapshot decoded{};
    if (!readEntityKinematicsSnapshot(rx.data(), nr2, decoded)) {
        return 7;
    }
    if (decoded.simTick != snap.simTick || decoded.entity.guid != snap.entity.guid) {
        return 8;
    }

    if (!client.send(1u, wire.data(), nw)) {
        return 9;
    }
    PeerId fromB{kInvalidPeerId};
    const std::size_t back = host.receive(fromB, rx.data(), rx.size());
    if (back != nw || fromB != 2u) {
        return 10;
    }

    for (std::size_t i = 0; i < 4; ++i) {
        if (!host.send(2u, wire.data(), nw)) {
            return 11;
        }
    }
    if (host.send(2u, wire.data(), nw)) {
        return 12;
    }

    return 0;
}
