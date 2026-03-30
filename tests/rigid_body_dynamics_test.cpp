#include "physics/RigidBodyDynamics.hpp"

namespace {

bool approx(float a, float b, float eps = 1e-5f) {
    return a - b <= eps && b - a <= eps;
}

bool approx3(marble::math::Vec3 v, float x, float y, float z) {
    return approx(v.x, x) && approx(v.y, y) && approx(v.z, z);
}

} // namespace

int main() {
    using marble::math::Vec3;
    using marble::physics::applyImpulseLinear;
    using marble::physics::integrateSemiImplicitEuler;
    using marble::physics::integrateYawAxis;
    using marble::physics::RigidBodyKinematics;
    using marble::physics::RigidBodyYaw;

    {
        RigidBodyKinematics b{};
        b.invMass = 1.f;
        integrateSemiImplicitEuler(b, Vec3{4.f, 0.f, 0.f}, 0.5f);
        if (!approx3(b.linearVelocity, 2.f, 0.f, 0.f) || !approx3(b.position, 1.f, 0.f, 0.f)) {
            return 1;
        }
    }

    {
        RigidBodyKinematics b{};
        b.invMass = 2.f;
        b.linearVelocity = {1.f, 0.f, 0.f};
        integrateSemiImplicitEuler(b, Vec3::zero(), 0.1f);
        if (!approx3(b.position, 0.1f, 0.f, 0.f)) {
            return 2;
        }
    }

    {
        RigidBodyKinematics staticBody{};
        staticBody.invMass = 0.f;
        integrateSemiImplicitEuler(staticBody, Vec3{100.f, 0.f, 0.f}, 1.f);
        if (!approx3(staticBody.linearVelocity, 0.f, 0.f, 0.f) || !approx3(staticBody.position, 0.f, 0.f, 0.f)) {
            return 3;
        }
    }

    {
        RigidBodyKinematics b{};
        b.invMass = 0.5f;
        applyImpulseLinear(b, Vec3{0.f, 6.f, 0.f});
        if (!approx3(b.linearVelocity, 0.f, 3.f, 0.f)) {
            return 4;
        }
    }

    {
        RigidBodyYaw y{};
        y.invInertiaY = 1.f;
        integrateYawAxis(y, 2.f, 0.25f);
        if (!approx(y.angularVelocityY, 0.5f) || !approx(y.yawRadians, 0.125f)) {
            return 5;
        }
    }

    {
        RigidBodyYaw y{};
        y.invInertiaY = 0.f;
        integrateYawAxis(y, 99.f, 1.f);
        if (!approx(y.angularVelocityY, 0.f) || !approx(y.yawRadians, 0.f)) {
            return 6;
        }
    }

    return 0;
}
