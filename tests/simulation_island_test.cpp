#include "gameplay/SimulationIsland.hpp"

#include <cmath>
#include <cstdlib>

namespace {

using marble::gameplay::SimulationIsland;
using marble::gameplay::WorldPosition3;
using marble::gameplay::localGameplayAabbToJolt;
using marble::gameplay::localGameplayHeightFieldToJolt;
using marble::gameplay::localGameplayToJolt;
using marble::gameplay::universePositionToJolt;
using marble::gameplay::worldPositionFromLocal;
using marble::math::Aabb;
using marble::math::Vec3;
using marble::physics::PhysicsBodyMaterial;
using marble::physics::PhysicsStaticHeightFieldDesc;

bool nearf(float a, float b, float eps) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    SimulationIsland const zero{};

    WorldPosition3 const w = worldPositionFromLocal(zero, Vec3{1.f, 2.f, -3.f});
    if (w.x != 1.0 || w.y != 2.0 || w.z != -3.0) {
        return 1;
    }

    Vec3 const j0 = localGameplayToJolt(zero, Vec3{12.5f, -1.f, 0.25f});
    if (!nearf(j0.x, 12.5f, 1e-4f) || !nearf(j0.y, -1.f, 1e-4f) || !nearf(j0.z, 0.25f, 1e-4f)) {
        return 2;
    }

    SimulationIsland const big{1.0e6, 0.0, 1.0e6};
    Vec3 const j1 = localGameplayToJolt(big, Vec3{5.f, 10.f, 3.f});
    if (!nearf(j1.x, 5.f, 0.02f) || !nearf(j1.y, 10.f, 0.02f) || !nearf(j1.z, 3.f, 0.02f)) {
        return 3;
    }

    WorldPosition3 const w1 = worldPositionFromLocal(big, Vec3{5.f, 0.f, 3.f});
    if (std::fabs(w1.x - (1.0e6 + 5.0)) > 1.0 || std::fabs(w1.z - (1.0e6 + 3.0)) > 1.0) {
        return 4;
    }

    // Float holds ~1e6+5 imprecisely; subtract origin in double should recover small offset.
    Vec3 const lossyUniverse{1000005.f, 20.f, 1000003.f};
    Vec3 const j2 = universePositionToJolt(big, lossyUniverse);
    if (!nearf(j2.x, 5.f, 2.f) || !nearf(j2.z, 3.f, 2.f)) {
        return 5;
    }

    Aabb const box{{-1.f, 0.f, 2.f}, {3.f, 4.f, 5.f}};
    Aabb const jb = localGameplayAabbToJolt(big, box);
    if (!nearf(jb.min.x, box.min.x, 0.05f) || !nearf(jb.max.z, box.max.z, 0.05f)) {
        return 6;
    }

    PhysicsStaticHeightFieldDesc hf{};
    hf.offset = Vec3{10.f, 0.f, -20.f};
    hf.scale = {2.f, 1.f, 2.f};
    hf.sampleCount = 2u;
    hf.material = PhysicsBodyMaterial{};
    float heights[4]{};
    hf.heights = heights;
    PhysicsStaticHeightFieldDesc const jh = localGameplayHeightFieldToJolt(big, hf);
    if (!nearf(jh.offset.x, hf.offset.x, 0.05f) || jh.sampleCount != hf.sampleCount) {
        return 7;
    }

    return 0;
}
