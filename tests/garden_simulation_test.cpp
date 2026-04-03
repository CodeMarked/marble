#include "garden/GardenSimulation.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsIntegration.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <span>

namespace {

using marble::garden::GardenLayout;
using marble::garden::buildGardenLayout;
using marble::garden::kGardenRadius;
using marble::garden::kMarbleRadius;
using marble::garden::placeMarblesInArena;
using marble::garden::rayHitsSphere;
using marble::garden::rayIntersectHorizontalPlane;
using marble::math::Vec3;
using marble::physics::PhysicsBodyMaterial;
using marble::physics::PhysicsCylindricalXZClamp;
using marble::physics::PhysicsDynamicSphereDesc;
using marble::physics::PhysicsStaticHeightFieldDesc;
using marble::physics::PhysicsStaticBoxDesc;
using marble::physics::PhysicsStepOptions;
using marble::physics::RigidBodyKinematics;
using marble::physics::SimplePhysicsWorld;
using marble::physics::createJoltPhysicsScene;

void rebuildGardenPhysicsScene(
    marble::physics::IPhysicsScene& scene,
    GardenLayout const& layout,
    std::array<RigidBodyKinematics, 2>& marbles,
    std::array<marble::physics::PhysicsBodyId, 2>& marbleBodyIds
) {
    scene.clear();
    PhysicsBodyMaterial staticMat{};
    staticMat.restitution = 0.30f;
    staticMat.friction = 0.55f;
    auto const& tr = layout.terrain;
    if (tr.sampleCount >= 2u && tr.heights.size() == static_cast<std::size_t>(tr.sampleCount) * tr.sampleCount) {
        PhysicsStaticHeightFieldDesc hf{};
        hf.offset = tr.origin;
        hf.scale = {tr.cellSize, 1.f, tr.cellSize};
        hf.sampleCount = tr.sampleCount;
        hf.heights = std::span<float const>(tr.heights.data(), tr.heights.size());
        hf.material = staticMat;
        (void)scene.addStaticHeightField(hf);
    }
    for (auto const& box : layout.staticColliders) {
        PhysicsStaticBoxDesc d{};
        d.bounds = box;
        d.material = staticMat;
        (void)scene.addStaticBox(d);
    }
    PhysicsBodyMaterial marbleMat{};
    marbleMat.restitution = 0.672f;
    marbleMat.friction = 0.42f;
    marbleMat.linearDamping = 0.02f;
    marbleMat.angularDamping = 0.10f;
    for (std::size_t i = 0; i < marbles.size(); ++i) {
        PhysicsDynamicSphereDesc sd{};
        sd.center = marbles[i].position;
        sd.linearVelocity = marbles[i].linearVelocity;
        sd.radius = kMarbleRadius;
        sd.invMass = marbles[i].invMass;
        sd.material = marbleMat;
        marbleBodyIds[i] = scene.addDynamicSphere(sd);
    }
    scene.optimizeBroadPhase();
}

} // namespace

int main() {
    GardenLayout layout{};
    buildGardenLayout(42u, layout);
    if (layout.terrain.sampleCount < 2u ||
        layout.terrain.heights.size() != static_cast<std::size_t>(layout.terrain.sampleCount) * layout.terrain.sampleCount) {
        return 7;
    }
    GardenLayout layoutDup{};
    buildGardenLayout(42u, layoutDup);
    if (layout.terrain.heights != layoutDup.terrain.heights) {
        return 8;
    }
    GardenLayout layoutOther{};
    buildGardenLayout(99991u, layoutOther);
    bool heightsDiffer = false;
    for (std::size_t i = 0; i < layout.terrain.heights.size(); i += 23) {
        if (std::fabs(layout.terrain.heights[i] - layoutOther.terrain.heights[i]) > 1e-3f) {
            heightsDiffer = true;
            break;
        }
    }
    if (!heightsDiffer) {
        return 9;
    }
    if (layout.staticColliders.empty() || layout.staticColliders.size() != layout.colliderTangentRetention.size() ||
        layout.kinds.size() != layout.staticColliders.size() ||
        layout.propMesh.size() != layout.staticColliders.size() ||
        layout.propYaw.size() != layout.staticColliders.size()) {
        return 1;
    }

    float t = 0.f;
    Vec3 rayO{0.f, 2.f, 3.f};
    Vec3 rayD{0.f, -0.5f, -1.f};
    if (!rayHitsSphere(rayO, rayD, Vec3::zero(), 1.f, t) || t < 0.f) {
        return 2;
    }

    Vec3 planeHit{};
    if (!rayIntersectHorizontalPlane(Vec3{0.f, 2.f, 0.f}, Vec3{0.3f, -1.f, 0.f}, 0.f, planeHit)) {
        return 5;
    }
    if (std::fabs(planeHit.y) > 1e-3f || std::fabs(planeHit.x - 0.6f) > 0.05f) {
        return 6;
    }

    std::array<RigidBodyKinematics, 2> marbles{};
    placeMarblesInArena(marbles, layout);
    SimplePhysicsWorld world{};
    world.setSettings({.gravity = {0.f, -9.81f, 0.f}, .maxSubSteps = 1u});
    auto scene = createJoltPhysicsScene();
    std::array<marble::physics::PhysicsBodyId, 2> marbleBodyIds{};
    rebuildGardenPhysicsScene(*scene, layout, marbles, marbleBodyIds);

    constexpr float dt = 1.f / 60.f;
    float const minCenterY = layout.terrain.minHeight - kMarbleRadius - 0.55f;
    PhysicsCylindricalXZClamp const clamp{
        kGardenRadius - kMarbleRadius - 0.02f,
        minCenterY,
    };
    PhysicsStepOptions const stepOpts{&clamp, std::span(marbleBodyIds)};
    for (int i = 0; i < 480; ++i) {
        scene->syncHostVelocitiesBeforeStep(marbleBodyIds, marbles.data(), marbles.size());
        scene->step(dt, world.settings(), stepOpts);
        scene->readBackKinematics(marbleBodyIds, marbles.data(), marbles.size());
    }

    for (auto const& m : marbles) {
        float const xz = std::sqrt(m.position.x * m.position.x + m.position.z * m.position.z);
        if (xz > kGardenRadius + 0.05f) {
            return 3;
        }
        if (m.position.y < layout.terrain.minHeight - 20.f || m.position.y > layout.terrain.maxHeight + 70.f) {
            return 4;
        }
    }

    return 0;
}
