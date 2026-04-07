#include "gameplay/InterestManagement.hpp"
#include "gameplay/AuthoritativeSession.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace marble::gameplay;
using marble::math::Vec3;

static void testFilterNearbyEntities() {
    ReplicatedEntity entities[4]{};
    entities[0] = {WorldObjectRef{1}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact, true};
    entities[1] = {WorldObjectRef{2}, {10.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact, true};
    entities[2] = {WorldObjectRef{3}, {500.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact, true};
    entities[3] = {WorldObjectRef{4}, {1000.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact, true};

    InterestRegion region{};
    region.viewPosition = {0.f, 0.f, 0.f};
    region.radius = 100.f;
    region.velocityLookaheadSeconds = 0.f;

    ReplicatedEntity out[4]{};
    std::size_t count = filterEntitiesForPeer<4>(entities, 4, region, out, 4);

    assert(count == 2);
    assert(out[0].entity == WorldObjectRef{1});
    assert(out[1].entity == WorldObjectRef{2});
    std::printf("  filtered %zu entities within radius 100 (expected 2)\n", count);
}

static void testFilterExcludesInactive() {
    ReplicatedEntity entities[2]{};
    entities[0] = {WorldObjectRef{1}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact, true};
    entities[1] = {WorldObjectRef{2}, {5.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, PhysicsSimulationTier::Contact, false};

    InterestRegion region{};
    region.viewPosition = {0.f, 0.f, 0.f};
    region.radius = 100.f;

    ReplicatedEntity out[2]{};
    std::size_t count = filterEntitiesForPeer<2>(entities, 2, region, out, 2);

    assert(count == 1);
    assert(out[0].entity == WorldObjectRef{1});
    std::printf("  inactive entities excluded: count=%zu (expected 1)\n", count);
}

static void testVelocityLookahead() {
    ReplicatedEntity entities[1]{};
    entities[0] = {WorldObjectRef{1}, {150.f, 0.f, 0.f}, {200.f, 0.f, 0.f}, PhysicsSimulationTier::Contact, true};

    InterestRegion region{};
    region.viewPosition = {0.f, 0.f, 0.f};
    region.radius = 100.f;
    region.velocityLookaheadSeconds = 0.f;

    ReplicatedEntity out[1]{};
    std::size_t count = filterEntitiesForPeer<1>(entities, 1, region, out, 1);
    assert(count == 0);
    std::printf("  fast mover without lookahead: excluded (count=%zu)\n", count);

    region.velocityLookaheadSeconds = 0.5f;
    count = filterEntitiesForPeer<1>(entities, 1, region, out, 1);
    assert(count == 1);
    std::printf("  fast mover with lookahead 0.5s: included (count=%zu, effective_r=200)\n", count);
}

static void testMaxOutLimit() {
    ReplicatedEntity entities[4]{};
    for (int i = 0; i < 4; ++i) {
        entities[i] = {WorldObjectRef{static_cast<std::uint64_t>(i + 1)},
                       {static_cast<float>(i), 0.f, 0.f},
                       {0.f, 0.f, 0.f},
                       PhysicsSimulationTier::Contact,
                       true};
    }

    InterestRegion region{};
    region.viewPosition = {0.f, 0.f, 0.f};
    region.radius = 100.f;

    ReplicatedEntity out[2]{};
    std::size_t count = filterEntitiesForPeer<4>(entities, 4, region, out, 2);
    assert(count == 2);
    std::printf("  maxOut=2 caps output: count=%zu (expected 2)\n", count);
}

static void testPeerInterestEntry() {
    PeerInterest pi{};
    pi.region.viewPosition = {10.f, 0.f, 0.f};
    pi.region.radius = 50.f;
    pi.viewEntityIndex = 3;
    pi.aoiEnabled = true;

    assert(pi.aoiEnabled);
    assert(pi.viewEntityIndex == 3);
    assert(pi.region.radius == 50.f);
    std::printf("  PeerInterest struct: OK\n");
}

int main() {
    std::printf("interest_management_test\n");

    std::printf("testFilterNearbyEntities:\n");
    testFilterNearbyEntities();

    std::printf("testFilterExcludesInactive:\n");
    testFilterExcludesInactive();

    std::printf("testVelocityLookahead:\n");
    testVelocityLookahead();

    std::printf("testMaxOutLimit:\n");
    testMaxOutLimit();

    std::printf("testPeerInterestEntry:\n");
    testPeerInterestEntry();

    std::printf("All interest_management_test tests passed.\n");
    return 0;
}
