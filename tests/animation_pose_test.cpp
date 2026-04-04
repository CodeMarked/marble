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
    using marble::animation::computeGlobalPose;
    using marble::animation::kInvalidParent;
    using marble::animation::skeletonParentsWellOrdered;
    using marble::math::Mat4;
    using marble::math::Point3;
    using marble::math::transformPoint;
    using marble::math::Vec3;

    {
        std::array<std::uint16_t, 1> parents{kInvalidParent};
        if (!skeletonParentsWellOrdered(1, parents.data())) {
            return 1;
        }
    }

    {
        std::array<std::uint16_t, 2> parents{kInvalidParent, 0};
        if (!skeletonParentsWellOrdered(2, parents.data())) {
            return 2;
        }
    }

    {
        std::array<std::uint16_t, 2> parents{0, kInvalidParent};
        if (skeletonParentsWellOrdered(2, parents.data())) {
            return 3;
        }
    }

    {
        std::array<std::uint16_t, 2> parents{kInvalidParent, 1};
        if (skeletonParentsWellOrdered(2, parents.data())) {
            return 4;
        }
    }

    std::array<std::uint16_t, 2> parents{kInvalidParent, 0};
    std::array<Mat4, 2> local{
        Mat4::translation(Vec3{1.f, 0.f, 0.f}),
        Mat4::translation(Vec3{0.f, 2.f, 0.f}),
    };
    std::array<Mat4, 2> global{};
    computeGlobalPose(2, parents.data(), local.data(), global.data());

    const Point3 childOriginWorld = transformPoint(global[1], Point3{0.f, 0.f, 0.f});
    if (!approx(childOriginWorld.x, 1.f) || !approx(childOriginWorld.y, 2.f) || !approx(childOriginWorld.z, 0.f)) {
        return 5;
    }

    std::array<std::uint16_t, 3> chainParents{kInvalidParent, 0, 1};
    std::array<Mat4, 3> chainLocal{
        Mat4::identity(),
        Mat4::translation(Vec3{0.f, 1.f, 0.f}),
        Mat4::translation(Vec3{0.f, 0.f, 1.f}),
    };
    std::array<Mat4, 3> chainGlobal{};
    computeGlobalPose(3, chainParents.data(), chainLocal.data(), chainGlobal.data());
    const Point3 tip = transformPoint(chainGlobal[2], Point3{0.f, 0.f, 0.f});
    if (!approx(tip.x, 0.f) || !approx(tip.y, 1.f) || !approx(tip.z, 1.f)) {
        return 6;
    }

    return 0;
}
