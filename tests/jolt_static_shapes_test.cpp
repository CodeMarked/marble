#include "core/MeshAssetV1.hpp"
#include "core/MeshAssetV1PhysicsExtract.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include "math/Vec3.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>
#include <memory>
#include <span>
#include <vector>

using namespace marble::physics;

namespace {

PhysicsBodyMaterial staticMat() noexcept {
    PhysicsBodyMaterial m{};
    m.restitution = 0.25f;
    m.friction = 0.55f;
    return m;
}

void testStaticSphereAndFloor() {
    auto scene = createJoltPhysicsScene();
    assert(scene != nullptr);

    PhysicsStaticBoxDesc floor{};
    floor.bounds = {{-20.f, -1.f, -20.f}, {20.f, 0.f, 20.f}};
    floor.material = staticMat();
    assert(scene->addStaticBox(floor) != kInvalidPhysicsBodyId);

    PhysicsStaticSphereDesc dome{};
    dome.center = {0.f, 0.35f, 0.f};
    dome.radius = 0.5f;
    dome.material = staticMat();
    assert(scene->addStaticSphere(dome) != kInvalidPhysicsBodyId);

    PhysicsDynamicSphereDesc ball{};
    ball.center = {0.f, 2.5f, 0.f};
    ball.linearVelocity = {};
    ball.radius = 0.11f;
    ball.invMass = 5.f;
    ball.material.restitution = 0.35f;
    ball.material.friction = 0.45f;
    PhysicsBodyId const bid = scene->addDynamicSphere(ball);
    assert(bid != kInvalidPhysicsBodyId);

    scene->optimizeBroadPhase();

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};
    for (int i = 0; i < 480; ++i) {
        scene->step(1.f / 60.f, settings);
    }

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&bid, 1), &kin, 1);
    assert(kin.position.y > 0.7f);
    assert(kin.position.y < 1.35f);
    std::printf("  static sphere + floor: ball y=%.3f\n", static_cast<double>(kin.position.y));
}

void testStaticCapsuleAndFloor() {
    auto scene = createJoltPhysicsScene();
    assert(scene != nullptr);

    PhysicsStaticBoxDesc floor{};
    floor.bounds = {{-20.f, -1.f, -20.f}, {20.f, 0.f, 20.f}};
    floor.material = staticMat();
    assert(scene->addStaticBox(floor) != kInvalidPhysicsBodyId);

    PhysicsStaticCapsuleDesc cap{};
    cap.center = {0.f, 1.05f, 0.f};
    cap.halfHeight = 0.45f;
    cap.radius = 0.28f;
    cap.yawRadians = 0.f;
    cap.pitchRadians = 0.f;
    cap.rollRadians = 0.f;
    cap.intrinsicCylinderAxis = 0u;
    cap.material = staticMat();
    assert(scene->addStaticCapsule(cap) != kInvalidPhysicsBodyId);

    PhysicsDynamicSphereDesc ball{};
    ball.center = {0.f, 3.2f, 0.f};
    ball.linearVelocity = {};
    ball.radius = 0.11f;
    ball.invMass = 4.f;
    ball.material.restitution = 0.3f;
    ball.material.friction = 0.5f;
    PhysicsBodyId const bid = scene->addDynamicSphere(ball);
    assert(bid != kInvalidPhysicsBodyId);

    scene->optimizeBroadPhase();

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};
    for (int i = 0; i < 480; ++i) {
        scene->step(1.f / 60.f, settings);
    }

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&bid, 1), &kin, 1);
    // Resting on capsule crown (~center.y + halfHeight + radius + marbleRadius) or on floor if tunneled.
    float const expectedCrown = cap.center.y + cap.halfHeight + cap.radius + ball.radius;
    assert(kin.position.y > 0.09f);
    assert(kin.position.y < expectedCrown + 0.25f);
    std::printf("  static capsule + floor: ball y=%.3f (crown ~%.3f)\n", static_cast<double>(kin.position.y),
        static_cast<double>(expectedCrown));
}

void testStaticOrientedBoxAndFloor() {
    auto scene = createJoltPhysicsScene();
    assert(scene != nullptr);

    PhysicsStaticBoxDesc floor{};
    floor.bounds = {{-20.f, -1.f, -20.f}, {20.f, 0.f, 20.f}};
    floor.material = staticMat();
    assert(scene->addStaticBox(floor) != kInvalidPhysicsBodyId);

    PhysicsStaticOrientedBoxDesc ob{};
    ob.center = {0.f, 0.55f, 0.f};
    ob.halfExtents = {0.75f, 0.08f, 0.3f};
    ob.yawRadians = 0.25f;
    ob.pitchRadians = 0.08f;
    ob.rollRadians = -0.15f;
    ob.material = staticMat();
    assert(scene->addStaticOrientedBox(ob) != kInvalidPhysicsBodyId);

    PhysicsDynamicSphereDesc ball{};
    ball.center = {0.f, 2.4f, 0.f};
    ball.linearVelocity = {};
    ball.radius = 0.11f;
    ball.invMass = 4.f;
    ball.material.restitution = 0.3f;
    ball.material.friction = 0.5f;
    PhysicsBodyId const bid = scene->addDynamicSphere(ball);
    assert(bid != kInvalidPhysicsBodyId);

    scene->optimizeBroadPhase();

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};
    for (int i = 0; i < 520; ++i) {
        scene->step(1.f / 60.f, settings);
    }

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&bid, 1), &kin, 1);
    assert(kin.position.y > 0.08f);
    assert(kin.position.y < 2.5f);
    std::printf("  static oriented box + floor: ball y=%.3f\n", static_cast<double>(kin.position.y));
}

void testStaticConvexHullBoxAndFloor() {
    auto scene = createJoltPhysicsScene();
    assert(scene != nullptr);

    PhysicsStaticBoxDesc floor{};
    floor.bounds = {{-20.f, -1.f, -20.f}, {20.f, 0.f, 20.f}};
    floor.material = staticMat();
    assert(scene->addStaticBox(floor) != kInvalidPhysicsBodyId);

    float const h = 0.5f;
    std::vector<marble::math::Vec3> corners{};
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                corners.push_back(marble::math::Vec3{static_cast<float>(sx) * h, static_cast<float>(sy) * h,
                    static_cast<float>(sz) * h});
            }
        }
    }
    assert(corners.size() == 8u);

    PhysicsStaticConvexHullDesc hull{};
    hull.center = {0.f, 0.65f, 0.f};
    hull.yawRadians = 0.12f;
    hull.pitchRadians = -0.07f;
    hull.rollRadians = 0.09f;
    hull.points = std::span<marble::math::Vec3 const>(corners.data(), corners.size());
    hull.material = staticMat();
    assert(scene->addStaticConvexHull(hull) != kInvalidPhysicsBodyId);

    PhysicsDynamicSphereDesc ball{};
    ball.center = {0.f, 2.8f, 0.f};
    ball.linearVelocity = {};
    ball.radius = 0.11f;
    ball.invMass = 4.f;
    ball.enhancedInternalEdgeRemoval = true;
    ball.material.restitution = 0.28f;
    ball.material.friction = 0.5f;
    PhysicsBodyId const bid = scene->addDynamicSphere(ball);
    assert(bid != kInvalidPhysicsBodyId);

    scene->optimizeBroadPhase();

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};
    for (int i = 0; i < 520; ++i) {
        scene->step(1.f / 60.f, settings);
    }

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&bid, 1), &kin, 1);
    assert(std::isfinite(kin.position.x) && std::isfinite(kin.position.y) && std::isfinite(kin.position.z));
    assert(kin.position.y > hull.center.y - 0.55f);
    assert(kin.position.y < hull.center.y + 0.55f + ball.radius + 0.2f);
    std::printf("  static convex hull + floor: ball y=%.3f\n", static_cast<double>(kin.position.y));
}

void testStaticTriangleMeshRampAndFloor() {
    auto scene = createJoltPhysicsScene();
    assert(scene != nullptr);

    PhysicsStaticBoxDesc floor{};
    floor.bounds = {{-20.f, -1.f, -20.f}, {20.f, 0.f, 20.f}};
    floor.material = staticMat();
    assert(scene->addStaticBox(floor) != kInvalidPhysicsBodyId);

    std::vector<std::uint8_t> const rampBytes = marble::core::meshAssetV1BuildDemoRampBytes();
    std::optional<marble::core::MeshAssetV1CpuViews> const views = marble::core::meshAssetV1TryParse(std::span(rampBytes));
    assert(views.has_value());

    std::vector<marble::math::Vec3> verts{};
    std::vector<std::uint32_t> indices{};
    assert(marble::core::meshAssetV1IndexedTriangleMeshForPhysics(*views, verts, indices));
    assert(!verts.empty() && indices.size() >= 3u);

    PhysicsStaticTriangleMeshDesc mesh{};
    mesh.center = {0.f, 0.02f, 0.f};
    mesh.yawRadians = 0.f;
    mesh.pitchRadians = 0.f;
    mesh.rollRadians = 0.f;
    mesh.vertices = std::span<marble::math::Vec3 const>(verts.data(), verts.size());
    mesh.indices = std::span<std::uint32_t const>(indices.data(), indices.size());
    mesh.enhancedInternalEdgeRemoval = true;
    mesh.material = staticMat();
    assert(scene->addStaticTriangleMesh(mesh) != kInvalidPhysicsBodyId);

    PhysicsDynamicSphereDesc ball{};
    ball.center = {0.6f, 0.55f, 0.35f};
    ball.linearVelocity = {0.f, 0.f, 0.f};
    ball.radius = 0.11f;
    ball.invMass = 5.f;
    ball.enhancedInternalEdgeRemoval = true;
    ball.material.restitution = 0.22f;
    ball.material.friction = 0.55f;
    PhysicsBodyId const bid = scene->addDynamicSphere(ball);
    assert(bid != kInvalidPhysicsBodyId);

    scene->optimizeBroadPhase();

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};
    for (int i = 0; i < 720; ++i) {
        scene->step(1.f / 60.f, settings);
    }

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&bid, 1), &kin, 1);
    assert(std::isfinite(kin.position.x) && std::isfinite(kin.position.y) && std::isfinite(kin.position.z));
    assert(kin.position.y > 0.08f);
    assert(kin.position.y < 1.5f);
    std::printf("  static triangle mesh ramp + floor: ball y=%.3f\n", static_cast<double>(kin.position.y));
}

} // namespace

int main() {
    std::printf("jolt_static_shapes_test:\n");
    testStaticSphereAndFloor();
    testStaticCapsuleAndFloor();
    testStaticOrientedBoxAndFloor();
    testStaticConvexHullBoxAndFloor();
    testStaticTriangleMeshRampAndFloor();
    std::printf("jolt_static_shapes_test: ok\n");
    return 0;
}
