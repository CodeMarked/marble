#include "math/Mat4.hpp"
#include "physics/PhysicsIntegration.hpp"

namespace {

bool approx(float a, float b, float eps = 1e-4f) {
    return a - b <= eps && b - a <= eps;
}

} // namespace

int main() {
    using marble::math::Mat4;
    using marble::math::Vec3;
    using marble::physics::IPhysicsWorld;
    using marble::physics::PhysicsWorldSettings;
    using marble::physics::RigidBodyKinematics;
    using marble::physics::rigidBodyTranslationMatrix;
    using marble::physics::SimplePhysicsWorld;

    SimplePhysicsWorld worldStorage{};
    IPhysicsWorld& world = worldStorage;
    PhysicsWorldSettings cfg{};
    cfg.gravity = {0.f, -10.f, 0.f};
    world.setSettings(cfg);

    RigidBodyKinematics bodies[2]{};
    bodies[0].invMass = 1.f;
    bodies[1].invMass = 0.f;
    bodies[1].position = {0.f, 5.f, 0.f};

    world.step(0.1f, bodies, 2);

    if (!approx(bodies[0].linearVelocity.y, -1.f) || !approx(bodies[0].position.y, -0.1f)) {
        return 1;
    }
    if (!approx(bodies[1].linearVelocity.y, 0.f) || !approx(bodies[1].position.y, 5.f)) {
        return 2;
    }

    PhysicsWorldSettings z{};
    z.gravity = Vec3::zero();
    world.setSettings(z);
    bodies[0] = {};
    bodies[0].invMass = 1.f;
    bodies[0].linearVelocity = {2.f, 0.f, 0.f};
    world.step(0.2f, bodies, 1);
    if (!approx(bodies[0].position.x, 0.4f)) {
        return 3;
    }

    {
        RigidBodyKinematics b{};
        b.position = {1.f, 2.f, 3.f};
        const Mat4 m = rigidBodyTranslationMatrix(b);
        const auto p = marble::math::transformPoint(m, marble::math::Point3{0.f, 0.f, 0.f});
        if (!approx(p.x, 1.f) || !approx(p.y, 2.f) || !approx(p.z, 3.f)) {
            return 4;
        }
    }

    {
        PhysicsWorldSettings s = world.settings();
        s.maxSubSteps = 2;
        s.enableContinuousCollision = true;
        world.setSettings(s);
        if (world.settings().maxSubSteps != 2 || !world.settings().enableContinuousCollision) {
            return 5;
        }
    }

    return 0;
}
