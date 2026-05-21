#include "gameplay/InterestManagement.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace marble::gameplay;
using marble::math::Vec3;

static void testFilterNearbyEntities() {
    ReplicatedEntity entities[4]{};
    entities[0] = {WorldObjectRef{1}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 0.f, PhysicsSimulationTier::Contact, true};
    entities[1] = {WorldObjectRef{2}, {10.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 0.f, PhysicsSimulationTier::Contact, true};
    entities[2] = {WorldObjectRef{3}, {500.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 0.f, PhysicsSimulationTier::Contact, true};
    entities[3] = {WorldObjectRef{4}, {1000.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 0.f, PhysicsSimulationTier::Contact, true};

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
    entities[0] = {WorldObjectRef{1}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 0.f, PhysicsSimulationTier::Contact, true};
    entities[1] = {WorldObjectRef{2}, {5.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 0.f, PhysicsSimulationTier::Contact, false};

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
    entities[0] = {WorldObjectRef{1}, {150.f, 0.f, 0.f}, {200.f, 0.f, 0.f}, 0.f, PhysicsSimulationTier::Contact, true};

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
                       0.f,
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

static void testInterestScoreZeroViewVelocity() {
    ReplicatedEntity e{};
    e.position = {3.f, 4.f, 0.f};
    e.velocity = {0.f, 0.f, 0.f};
    InterestViewContext view{};
    view.position = {0.f, 0.f, 0.f};
    view.velocity = {0.f, 0.f, 0.f};
    float const s = interestScoreLowerIsBetter(e, view);
    float const dist2 = 25.f; // 3*3+4*4
    assert(std::fabs(s - dist2) < 1e-5f);
    std::printf("  zero view vel: score == dist2 (25)\n");
}

static void testInterestScoreAheadVsBehind() {
    InterestViewContext view{};
    view.position = {0.f, 0.f, 0.f};
    view.velocity = {10.f, 0.f, 0.f};
    ReplicatedEntity ahead{};
    ahead.position = {50.f, 0.f, 0.f};
    ahead.velocity = {0.f, 0.f, 0.f};
    ReplicatedEntity behind{};
    behind.position = {-50.f, 0.f, 0.f};
    behind.velocity = {0.f, 0.f, 0.f};
    float const sa = interestScoreLowerIsBetter(ahead, view);
    float const sb = interestScoreLowerIsBetter(behind, view);
    assert(sb > sa);
    std::printf("  ahead vs behind: behind score higher (worse)\n");
}

static void testInterestScoreEntitySpeedPreference() {
    InterestViewContext view{};
    view.position = {0.f, 0.f, 0.f};
    view.velocity = {0.f, 0.f, 0.f};
    ReplicatedEntity slow{};
    slow.position = {10.f, 0.f, 0.f};
    slow.velocity = {0.f, 0.f, 0.f};
    ReplicatedEntity fast{};
    fast.position = {10.f, 0.f, 0.f};
    fast.velocity = {1000.f, 0.f, 0.f};
    float const sSlow = interestScoreLowerIsBetter(slow, view);
    float const sFast = interestScoreLowerIsBetter(fast, view);
    assert(sSlow > sFast);
    std::printf("  same dist: faster entity gets lower (better) score\n");
}

static void testQuantizedKinematicsFingerprint() {
    ReplicatedEntity a{};
    a.position = {1.f, 0.f, 0.f};
    a.velocity = {0.f, 0.f, 0.f};
    a.yawRadians = 0.f;
    ReplicatedEntity b = a;
    assert(quantizedKinematicsFingerprint(a) == quantizedKinematicsFingerprint(b));

    ReplicatedEntity c = a;
    c.position.x = 1.f + (1.f / 48.f) + 0.02f; // crosses one quantization step of 1/48 m
    assert(quantizedKinematicsFingerprint(a) != quantizedKinematicsFingerprint(c));

    // Golden ties to InterestManagement.hpp quantization (lround(x*48)); update if that changes.
    std::uint32_t const golden = 0x2b7a4cf5u;
    assert(quantizedKinematicsFingerprint(a) == golden);

    std::printf("  fingerprint: identical, different on quant step, golden OK\n");
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

    std::printf("testInterestScoreZeroViewVelocity:\n");
    testInterestScoreZeroViewVelocity();

    std::printf("testInterestScoreAheadVsBehind:\n");
    testInterestScoreAheadVsBehind();

    std::printf("testInterestScoreEntitySpeedPreference:\n");
    testInterestScoreEntitySpeedPreference();

    std::printf("testQuantizedKinematicsFingerprint:\n");
    testQuantizedKinematicsFingerprint();

    std::printf("All interest_management_test tests passed.\n");
    return 0;
}
