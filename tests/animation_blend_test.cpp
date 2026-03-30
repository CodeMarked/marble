#include "animation/AnimationBlend.hpp"
#include "animation/SkeletonPose.hpp"
#include "math/Mat4.hpp"

#include <array>
#include <cstdint>

namespace {

bool approx(float a, float b, float eps = 1e-5f) {
    return a - b <= eps && b - a <= eps;
}

} // namespace

int main() {
    using marble::animation::addLocalJointTranslation;
    using marble::animation::blendLocalPoses;
    using marble::animation::blendWeightClamp;
    using marble::animation::computeGlobalPose;
    using marble::animation::kInvalidParent;
    using marble::animation::preMultiplyLocalJoint;
    using marble::math::Mat4;
    using marble::math::Point3;
    using marble::math::transformPoint;
    using marble::math::Vec3;

    if (!approx(blendWeightClamp(-1.f), 0.f) || !approx(blendWeightClamp(2.f), 1.f)) {
        return 1;
    }

    std::array<Mat4, 1> a{Mat4::translation(Vec3{0.f, 0.f, 0.f})};
    std::array<Mat4, 1> b{Mat4::translation(Vec3{10.f, 0.f, 0.f})};
    std::array<Mat4, 1> out{};

    blendLocalPoses(1, 0.f, a.data(), b.data(), out.data());
    Point3 p0 = transformPoint(out[0], Point3{0.f, 0.f, 0.f});
    if (!approx(p0.x, 0.f)) {
        return 2;
    }

    blendLocalPoses(1, 1.f, a.data(), b.data(), out.data());
    Point3 p1 = transformPoint(out[0], Point3{0.f, 0.f, 0.f});
    if (!approx(p1.x, 10.f)) {
        return 3;
    }

    blendLocalPoses(1, 0.25f, a.data(), b.data(), out.data());
    Point3 pm = transformPoint(out[0], Point3{0.f, 0.f, 0.f});
    if (!approx(pm.x, 2.5f)) {
        return 4;
    }

    std::array<Mat4, 1> local{Mat4::translation(Vec3{1.f, 0.f, 0.f})};
    addLocalJointTranslation(0, Vec3{0.f, 3.f, 0.f}, 1, local.data());
    Point3 pt = transformPoint(local[0], Point3{0.f, 0.f, 0.f});
    if (!approx(pt.x, 1.f) || !approx(pt.y, 3.f) || !approx(pt.z, 0.f)) {
        return 5;
    }

    std::array<Mat4, 1> lm{Mat4::translation(Vec3{1.f, 0.f, 0.f})};
    preMultiplyLocalJoint(0, Mat4::translation(Vec3{0.f, 2.f, 0.f}), 1, lm.data());
    Point3 pm2 = transformPoint(lm[0], Point3{0.f, 0.f, 0.f});
    if (!approx(pm2.x, 1.f) || !approx(pm2.y, 2.f) || !approx(pm2.z, 0.f)) {
        return 6;
    }

    std::array<std::uint16_t, 2> parents{kInvalidParent, 0};
    std::array<Mat4, 2> chain{
        Mat4::identity(),
        Mat4::translation(Vec3{0.f, 1.f, 0.f}),
    };
    preMultiplyLocalJoint(1, Mat4::translation(Vec3{0.f, 0.f, 0.5f}), 2, chain.data());
    std::array<Mat4, 2> glob{};
    computeGlobalPose(2, parents.data(), chain.data(), glob.data());
    const Point3 tip = transformPoint(glob[1], Point3{0.f, 0.f, 0.f});
    if (!approx(tip.x, 0.f) || !approx(tip.y, 1.f) || !approx(tip.z, 0.5f)) {
        return 7;
    }

    return 0;
}
