#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/CollisionMiddleware.hpp"
#include "physics/PhysicsWorld.hpp"

#include <array>
#include <cmath>
#include <cstdio>

using marble::math::Aabb;
using marble::math::Vec3;
using marble::physics::CollisionFilter;
using marble::physics::PhysicsBodyMaterial;
using marble::physics::PhysicsDynamicSphereDesc;
using marble::physics::PhysicsStaticBoxDesc;
using marble::physics::PhysicsWorldSettings;
using marble::physics::RigidBodyKinematics;
using marble::physics::createJoltPhysicsScene;

int main() {
    PhysicsBodyMaterial floorMat{};
    floorMat.restitution = 0.1f;
    floorMat.friction = 0.8f;

    PhysicsBodyMaterial sphereMat{};
    sphereMat.restitution = 0.15f;
    sphereMat.friction = 0.6f;
    sphereMat.linearDamping = 0.05f;

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};
    settings.maxSubSteps = 1;
    settings.enableSleeping = false;

    constexpr float dt = 1.f / 60.f;

    // --- Test 1: default filters — sphere collides with floor ---
    float yWithCollision{};
    {
        auto scene = createJoltPhysicsScene();

        PhysicsStaticBoxDesc floor{};
        floor.bounds = Aabb{{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
        floor.material = floorMat;
        (void)scene->addStaticBox(floor);

        PhysicsDynamicSphereDesc sd{};
        sd.center = {0.f, 5.f, 0.f};
        sd.radius = 0.3f;
        sd.invMass = 1.f;
        sd.material = sphereMat;
        auto bodyId = scene->addDynamicSphere(sd);

        scene->optimizeBroadPhase();

        for (int i = 0; i < 300; ++i) {
            scene->step(dt, settings);
        }

        RigidBodyKinematics k{};
        auto ids = std::array{bodyId};
        scene->readBackKinematics(ids, &k, 1);
        yWithCollision = k.position.y;

        if (yWithCollision < -0.1f || yWithCollision > 1.0f) {
            std::fprintf(stderr, "default filter: sphere at unexpected y=%.4f\n",
                static_cast<double>(yWithCollision));
            return 1;
        }
    }

    // --- Test 2: non-overlapping filters — sphere falls through floor ---
    {
        auto scene = createJoltPhysicsScene();

        constexpr std::uint32_t kLayerFloor = 0x0001u;
        constexpr std::uint32_t kLayerBall  = 0x0002u;

        PhysicsStaticBoxDesc floor{};
        floor.bounds = Aabb{{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
        floor.material = floorMat;
        floor.filter = CollisionFilter{kLayerFloor, kLayerFloor};
        (void)scene->addStaticBox(floor);

        PhysicsDynamicSphereDesc sd{};
        sd.center = {0.f, 5.f, 0.f};
        sd.radius = 0.3f;
        sd.invMass = 1.f;
        sd.material = sphereMat;
        sd.filter = CollisionFilter{kLayerBall, kLayerBall};
        auto bodyId = scene->addDynamicSphere(sd);

        scene->optimizeBroadPhase();

        for (int i = 0; i < 300; ++i) {
            scene->step(dt, settings);
        }

        RigidBodyKinematics k{};
        auto ids = std::array{bodyId};
        scene->readBackKinematics(ids, &k, 1);

        if (k.position.y > -5.f) {
            std::fprintf(stderr, "non-overlapping filter: sphere did NOT fall through (y=%.4f)\n",
                static_cast<double>(k.position.y));
            return 2;
        }
    }

    // --- Test 3: overlapping filters — sphere collides with floor ---
    {
        auto scene = createJoltPhysicsScene();

        constexpr std::uint32_t kLayerSolid = 0x0001u;

        PhysicsStaticBoxDesc floor{};
        floor.bounds = Aabb{{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
        floor.material = floorMat;
        floor.filter = CollisionFilter{kLayerSolid, 0xFFFFFFFFu};
        (void)scene->addStaticBox(floor);

        PhysicsDynamicSphereDesc sd{};
        sd.center = {0.f, 5.f, 0.f};
        sd.radius = 0.3f;
        sd.invMass = 1.f;
        sd.material = sphereMat;
        sd.filter = CollisionFilter{kLayerSolid, 0xFFFFFFFFu};
        auto bodyId = scene->addDynamicSphere(sd);

        scene->optimizeBroadPhase();

        for (int i = 0; i < 300; ++i) {
            scene->step(dt, settings);
        }

        RigidBodyKinematics k{};
        auto ids = std::array{bodyId};
        scene->readBackKinematics(ids, &k, 1);

        if (k.position.y < -0.1f || k.position.y > 1.0f) {
            std::fprintf(stderr, "overlapping filter: sphere at unexpected y=%.4f\n",
                static_cast<double>(k.position.y));
            return 3;
        }
    }

    // --- Test 4: two spheres, one pair collides and one doesn't ---
    {
        auto scene = createJoltPhysicsScene();

        constexpr std::uint32_t kLayerA = 0x0001u;
        constexpr std::uint32_t kLayerB = 0x0002u;

        PhysicsStaticBoxDesc floor{};
        floor.bounds = Aabb{{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
        floor.material = floorMat;
        floor.filter = CollisionFilter{kLayerA | kLayerB, kLayerA | kLayerB};
        (void)scene->addStaticBox(floor);

        PhysicsDynamicSphereDesc sdA{};
        sdA.center = {-1.f, 2.f, 0.f};
        sdA.linearVelocity = {3.f, 0.f, 0.f};
        sdA.radius = 0.3f;
        sdA.invMass = 1.f;
        sdA.material = sphereMat;
        sdA.filter = CollisionFilter{kLayerA, kLayerA};
        auto bodyA = scene->addDynamicSphere(sdA);

        PhysicsDynamicSphereDesc sdB{};
        sdB.center = {1.f, 2.f, 0.f};
        sdB.linearVelocity = {-3.f, 0.f, 0.f};
        sdB.radius = 0.3f;
        sdB.invMass = 1.f;
        sdB.material = sphereMat;
        sdB.filter = CollisionFilter{kLayerB, kLayerB};
        auto bodyB = scene->addDynamicSphere(sdB);

        scene->optimizeBroadPhase();

        for (int i = 0; i < 120; ++i) {
            scene->step(dt, settings);
        }

        RigidBodyKinematics kA{}, kB{};
        auto idsA = std::array{bodyA};
        auto idsB = std::array{bodyB};
        scene->readBackKinematics(idsA, &kA, 1);
        scene->readBackKinematics(idsB, &kB, 1);

        // A started left (-1) moving right, B started right (1) moving left.
        // With non-overlapping filters they should pass through each other:
        // A ends up right of where B started, B ends up left of where A started.
        if (kA.position.x < 0.5f || kB.position.x > -0.5f) {
            std::fprintf(stderr,
                "sphere-pass: spheres should have passed through each other "
                "(posA.x=%.4f, posB.x=%.4f)\n",
                static_cast<double>(kA.position.x),
                static_cast<double>(kB.position.x));
            return 4;
        }
    }

    return 0;
}
