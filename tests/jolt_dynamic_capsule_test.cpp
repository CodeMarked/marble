#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <cassert>
#include <cstdio>
#include <memory>

using namespace marble::physics;
using marble::math::Vec3;

static void testAddDynamicCapsule() {
    auto scene = createJoltPhysicsScene();
    assert(scene != nullptr);

    PhysicsDynamicCapsuleDesc desc{};
    desc.center = {0.f, 5.f, 0.f};
    desc.linearVelocity = {0.f, 0.f, 0.f};
    desc.halfHeight = 0.5f;
    desc.radius = 0.25f;
    desc.invMass = 1.f;
    PhysicsBodyId id = scene->addDynamicCapsule(desc);
    assert(id != kInvalidPhysicsBodyId);

    scene->optimizeBroadPhase();

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};

    RigidBodyKinematics kin{};
    kin.position = desc.center;

    for (int i = 0; i < 60; ++i) {
        scene->step(1.f / 60.f, settings);
    }

    scene->readBackKinematics(std::span(&id, 1), &kin, 1);
    assert(kin.position.y < 5.f);
    std::printf("  capsule fell to y=%.3f (expected < 5.0)\n", static_cast<double>(kin.position.y));
}

static void testCapsuleOnFloor() {
    auto scene = createJoltPhysicsScene();

    PhysicsStaticBoxDesc floor{};
    floor.bounds = {{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
    assert(scene->addStaticBox(floor) != kInvalidPhysicsBodyId);

    PhysicsDynamicCapsuleDesc desc{};
    desc.center = {0.f, 2.f, 0.f};
    desc.halfHeight = 0.4f;
    desc.radius = 0.2f;
    desc.invMass = 2.f;
    PhysicsBodyId id = scene->addDynamicCapsule(desc);
    assert(id != kInvalidPhysicsBodyId);

    scene->optimizeBroadPhase();

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};

    for (int i = 0; i < 300; ++i) {
        scene->step(1.f / 60.f, settings);
    }

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&id, 1), &kin, 1);

    assert(kin.position.y > -0.1f);
    assert(kin.position.y < 2.5f);
    std::printf("  capsule on floor y=%.3f (expected ~0.6)\n", static_cast<double>(kin.position.y));
}

static void testApplyLinearImpulse() {
    auto scene = createJoltPhysicsScene();

    PhysicsStaticBoxDesc floor{};
    floor.bounds = {{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
    assert(scene->addStaticBox(floor) != kInvalidPhysicsBodyId);

    PhysicsDynamicCapsuleDesc desc{};
    desc.center = {0.f, 1.f, 0.f};
    desc.halfHeight = 0.3f;
    desc.radius = 0.15f;
    desc.invMass = 1.f;
    PhysicsBodyId id = scene->addDynamicCapsule(desc);
    scene->optimizeBroadPhase();

    scene->applyLinearImpulse(id, {10.f, 0.f, 0.f});

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};

    scene->step(1.f / 60.f, settings);

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&id, 1), &kin, 1);
    assert(kin.linearVelocity.x > 0.1f);
    std::printf("  impulse applied: vx=%.3f (expected > 0.1)\n", static_cast<double>(kin.linearVelocity.x));
}

static void testSetBodyLinearVelocity() {
    auto scene = createJoltPhysicsScene();

    PhysicsDynamicSphereDesc desc{};
    desc.center = {0.f, 5.f, 0.f};
    desc.radius = 0.5f;
    desc.invMass = 1.f;
    PhysicsBodyId id = scene->addDynamicSphere(desc);
    scene->optimizeBroadPhase();

    scene->setBodyLinearVelocity(id, {0.f, 10.f, 0.f});

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, 0.f, 0.f};
    scene->step(1.f / 60.f, settings);

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&id, 1), &kin, 1);
    assert(kin.linearVelocity.y > 9.f);
    assert(kin.position.y > 5.f);
    std::printf("  setBodyLinearVelocity: vy=%.3f, y=%.3f\n",
        static_cast<double>(kin.linearVelocity.y), static_cast<double>(kin.position.y));
}

static void testApplyImpulseToSphere() {
    auto scene = createJoltPhysicsScene();

    PhysicsDynamicSphereDesc desc{};
    desc.center = {0.f, 5.f, 0.f};
    desc.radius = 0.5f;
    desc.invMass = 1.f;
    PhysicsBodyId id = scene->addDynamicSphere(desc);
    scene->optimizeBroadPhase();

    scene->applyLinearImpulse(id, {0.f, 5.f, 0.f});

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, 0.f, 0.f};
    scene->step(1.f / 60.f, settings);

    RigidBodyKinematics kin{};
    scene->readBackKinematics(std::span(&id, 1), &kin, 1);
    assert(kin.linearVelocity.y > 0.f);
    std::printf("  sphere impulse: vy=%.3f\n", static_cast<double>(kin.linearVelocity.y));
}

int main() {
    std::printf("jolt_dynamic_capsule_test\n");

    std::printf("testAddDynamicCapsule:\n");
    testAddDynamicCapsule();

    std::printf("testCapsuleOnFloor:\n");
    testCapsuleOnFloor();

    std::printf("testApplyLinearImpulse:\n");
    testApplyLinearImpulse();

    std::printf("testSetBodyLinearVelocity:\n");
    testSetBodyLinearVelocity();

    std::printf("testApplyImpulseToSphere:\n");
    testApplyImpulseToSphere();

    std::printf("All jolt_dynamic_capsule_test tests passed.\n");
    return 0;
}
