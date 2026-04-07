#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"

#include <cmath>
#include <cstdio>

using marble::math::Aabb;
using marble::math::Vec3;
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
    sphereMat.linearDamping = 0.1f;
    sphereMat.angularDamping = 0.2f;

    PhysicsWorldSettings settings{};
    settings.gravity = {0.f, -9.81f, 0.f};
    settings.maxSubSteps = 1;

    constexpr float dt = 1.f / 60.f;

    // --- Test 1: sleeping enabled, sphere should settle ---
    {
        auto scene = createJoltPhysicsScene();

        PhysicsStaticBoxDesc floor{};
        floor.bounds = Aabb{{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
        floor.material = floorMat;
        (void)scene->addStaticBox(floor);

        PhysicsDynamicSphereDesc sd{};
        sd.center = {0.f, 2.f, 0.f};
        sd.radius = 0.3f;
        sd.invMass = 1.f;
        sd.material = sphereMat;
        auto bodyId = scene->addDynamicSphere(sd);

        scene->optimizeBroadPhase();

        settings.enableSleeping = true;
        for (int i = 0; i < 600; ++i) {
            scene->step(dt, settings);
        }

        RigidBodyKinematics k{};
        auto ids = std::array{bodyId};
        scene->readBackKinematics(ids, &k, 1);

        float const speed = std::sqrt(
            k.linearVelocity.x * k.linearVelocity.x +
            k.linearVelocity.y * k.linearVelocity.y +
            k.linearVelocity.z * k.linearVelocity.z
        );
        if (speed > 0.05f) {
            std::fprintf(stderr, "sleeping enabled: sphere still moving (speed=%.4f)\n", static_cast<double>(speed));
            return 1;
        }
        if (k.position.y < -0.1f || k.position.y > 1.0f) {
            std::fprintf(stderr, "sleeping enabled: sphere at unexpected height (y=%.4f)\n", static_cast<double>(k.position.y));
            return 2;
        }
    }

    // --- Test 2: sleeping disabled, sphere should still settle (just stays awake) ---
    {
        auto scene = createJoltPhysicsScene();

        PhysicsStaticBoxDesc floor{};
        floor.bounds = Aabb{{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
        floor.material = floorMat;
        (void)scene->addStaticBox(floor);

        PhysicsDynamicSphereDesc sd{};
        sd.center = {0.f, 2.f, 0.f};
        sd.radius = 0.3f;
        sd.invMass = 1.f;
        sd.material = sphereMat;
        auto bodyId = scene->addDynamicSphere(sd);

        scene->optimizeBroadPhase();

        settings.enableSleeping = false;
        for (int i = 0; i < 600; ++i) {
            scene->step(dt, settings);
        }

        RigidBodyKinematics k{};
        auto ids = std::array{bodyId};
        scene->readBackKinematics(ids, &k, 1);

        if (k.position.y < -0.1f || k.position.y > 1.0f) {
            std::fprintf(stderr, "sleeping disabled: sphere at unexpected height (y=%.4f)\n", static_cast<double>(k.position.y));
            return 3;
        }
    }

    // --- Test 3: toggle sleeping on then off, body should wake ---
    {
        auto scene = createJoltPhysicsScene();

        PhysicsStaticBoxDesc floor{};
        floor.bounds = Aabb{{-10.f, -1.f, -10.f}, {10.f, 0.f, 10.f}};
        floor.material = floorMat;
        (void)scene->addStaticBox(floor);

        PhysicsDynamicSphereDesc sd{};
        sd.center = {0.f, 2.f, 0.f};
        sd.radius = 0.3f;
        sd.invMass = 1.f;
        sd.material = sphereMat;
        auto bodyId = scene->addDynamicSphere(sd);

        scene->optimizeBroadPhase();

        settings.enableSleeping = true;
        for (int i = 0; i < 600; ++i) {
            scene->step(dt, settings);
        }

        RigidBodyKinematics k{};
        auto ids = std::array{bodyId};
        scene->readBackKinematics(ids, &k, 1);

        Vec3 const posAfterSleep = k.position;

        settings.enableSleeping = false;
        scene->setBodyCenterAndLinearVelocity(bodyId, {0.f, 2.f, 0.f}, {0.f, 0.f, 0.f});

        for (int i = 0; i < 120; ++i) {
            scene->step(dt, settings);
        }

        scene->readBackKinematics(ids, &k, 1);
        if (k.position.y > 1.8f) {
            std::fprintf(stderr, "toggle: body did not fall after wake (y=%.4f)\n", static_cast<double>(k.position.y));
            return 4;
        }
        (void)posAfterSleep;
    }

    return 0;
}
