#include "gameplay/MultiplayerWireFormat.hpp"

#include <array>
#include <cmath>
#include <cstddef>

int main() {
    using namespace marble::gameplay;

    SimulationIsland island{};
    island.originX = 1e6;
    island.originY = -2.5;
    island.originZ = 42.25;

    std::array<std::uint8_t, 64> buf{};

    if (writeSimulationIslandAnchor(buf.data(), 10u, island) != 0u) {
        return 1;
    }
    const std::size_t nIsland = writeSimulationIslandAnchor(buf.data(), buf.size(), island);
    if (nIsland != kSimulationIslandAnchorWireBytes) {
        return 2;
    }
    SimulationIsland round{};
    if (!readSimulationIslandAnchor(buf.data(), nIsland, round)) {
        return 3;
    }
    if (round.originX != island.originX || round.originY != island.originY || round.originZ != island.originZ) {
        return 4;
    }

    TierHandoffAuthoritative handoff{};
    handoff.simTick = 9001u;
    handoff.entity = {404u};
    handoff.tier = PhysicsSimulationTier::CruiseOrbit;
    handoff.worldPosition = {1000.0, 2000.0, 3000.0};
    handoff.linearVelocity = {1.f, -2.f, 3.5f};

    if (writeTierHandoffAuthoritative(buf.data(), kTierHandoffWireBytes - 1u, handoff) != 0u) {
        return 5;
    }
    const std::size_t nHand = writeTierHandoffAuthoritative(buf.data(), buf.size(), handoff);
    if (nHand != kTierHandoffWireBytes) {
        return 6;
    }
    TierHandoffAuthoritative hand2{};
    if (!readTierHandoffAuthoritative(buf.data(), nHand, hand2)) {
        return 7;
    }
    if (hand2.simTick != handoff.simTick || hand2.entity.guid != handoff.entity.guid || hand2.tier != handoff.tier) {
        return 8;
    }
    if (hand2.worldPosition.x != handoff.worldPosition.x || hand2.linearVelocity.y != handoff.linearVelocity.y) {
        return 9;
    }

    EntityKinematicsSnapshot snap{};
    snap.simTick = 7u;
    snap.entity = {99u};
    snap.tier = PhysicsSimulationTier::Contact;
    snap.positionLocal = {10.f, 20.f, 30.f};
    snap.linearVelocity = {0.1f, 0.2f, 0.3f};

    if (writeEntityKinematicsSnapshot(buf.data(), kEntityKinematicsSnapshotWireBytes - 1u, snap) != 0u) {
        return 10;
    }
    const std::size_t nSnap = writeEntityKinematicsSnapshot(buf.data(), buf.size(), snap);
    if (nSnap != kEntityKinematicsSnapshotWireBytes) {
        return 11;
    }
    EntityKinematicsSnapshot snap2{};
    if (!readEntityKinematicsSnapshot(buf.data(), nSnap, snap2)) {
        return 12;
    }
    if (snap2.simTick != snap.simTick || snap2.entity.guid != snap.entity.guid) {
        return 13;
    }
    if (std::fabs(snap2.positionLocal.x - 10.f) > 1e-5f || std::fabs(snap2.linearVelocity.z - 0.3f) > 1e-5f) {
        return 14;
    }

    if (readSimulationIslandAnchor(buf.data(), kSimulationIslandAnchorWireBytes - 1u, round)) {
        return 15;
    }
    if (readTierHandoffAuthoritative(buf.data(), kTierHandoffWireBytes - 1u, hand2)) {
        return 16;
    }
    if (readEntityKinematicsSnapshot(buf.data(), kEntityKinematicsSnapshotWireBytes - 1u, snap2)) {
        return 17;
    }

    return 0;
}
