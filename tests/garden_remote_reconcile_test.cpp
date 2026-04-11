// Unit checks for Garden remote helpers: kinematic integration (optional) and emergency-only authority snap.

#include "garden/GardenRemoteReconcile.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using marble::math::Vec3;

namespace {

void testIntegrate() {
    Vec3 p{1.f, 2.f, 3.f};
    Vec3 v{10.f, 0.f, -5.f};
    assert(!marble::garden::integrateKinematicPositionTick(p, v, 0.f));
    assert(!marble::garden::integrateKinematicPositionTick(p, v, -1.f));
    assert(marble::garden::integrateKinematicPositionTick(p, v, 0.1f));
    assert(std::abs(p.x - 2.f) < 1e-5f);
    assert(std::abs(p.y - 2.f) < 1e-5f);
    assert(std::abs(p.z - 2.5f) < 1e-5f);
}

void testEmergencySnap() {
    constexpr float kThresh = 0.55f;
    {
        Vec3 p{0.f, 0.f, 0.f};
        Vec3 const auth{0.2f, 0.f, 0.f};
        auto const r = marble::garden::reconcileEmergencyPositionSnap(p, auth, kThresh);
        assert(!r.hardSnapped);
        assert(std::abs(p.x) < 1e-5f);
    }
    {
        Vec3 p{0.f, 0.f, 0.f};
        Vec3 const auth{0.6f, 0.f, 0.f};
        auto const r = marble::garden::reconcileEmergencyPositionSnap(p, auth, kThresh);
        assert(r.hardSnapped);
        assert(std::abs(p.x - 0.6f) < 1e-5f);
    }
}

} // namespace

int main() {
    testIntegrate();
    testEmergencySnap();
    std::puts("garden_remote_reconcile_test: ok");
    return 0;
}
