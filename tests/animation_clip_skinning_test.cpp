#include "animation/AnimationClip.hpp"
#include "animation/Skinning.hpp"
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
    using marble::animation::computeMatrixPalette;
    using marble::animation::kInvalidParent;
    using marble::animation::lerpMat4Elements;
    using marble::animation::sampleClipLocalPoses;
    using marble::math::Mat4;
    using marble::math::Point3;
    using marble::math::transformPoint;
    using marble::math::Vec3;

    {
        const Mat4 a = Mat4::translation(Vec3{0.f, 0.f, 0.f});
        const Mat4 b = Mat4::translation(Vec3{4.f, 0.f, 0.f});
        const Mat4 m = lerpMat4Elements(a, b, 0.25f);
        const Point3 p = transformPoint(m, Point3{0.f, 0.f, 0.f});
        if (!approx(p.x, 1.f) || !approx(p.y, 0.f) || !approx(p.z, 0.f)) {
            return 1;
        }
    }

    {
        std::array<Mat4, 2> keys{
            Mat4::translation(Vec3{0.f, 0.f, 0.f}),
            Mat4::translation(Vec3{2.f, 0.f, 0.f}),
        };
        std::array<Mat4, 1> local{};
        sampleClipLocalPoses(0.25f, 1.f, 1, 2, keys.data(), local.data());
        const Point3 p = transformPoint(local[0], Point3{0.f, 0.f, 0.f});
        if (!approx(p.x, 0.5f) || !approx(p.y, 0.f) || !approx(p.z, 0.f)) {
            return 2;
        }
    }

    {
        std::array<Mat4, 1> keys{Mat4::translation(Vec3{3.f, 0.f, 0.f})};
        std::array<Mat4, 1> local{};
        sampleClipLocalPoses(99.f, 1.f, 1, 1, keys.data(), local.data());
        const Point3 p = transformPoint(local[0], Point3{0.f, 0.f, 0.f});
        if (!approx(p.x, 3.f)) {
            return 3;
        }
    }

    {
        std::array<std::uint16_t, 1> parents{kInvalidParent};
        std::array<Mat4, 1> local{Mat4::translation(Vec3{1.f, 0.f, 0.f})};
        std::array<Mat4, 1> global{};
        computeGlobalPose(1, parents.data(), local.data(), global.data());

        std::array<Mat4, 1> invBind{Mat4::identity()};
        std::array<Mat4, 1> palette{};
        computeMatrixPalette(1, global.data(), invBind.data(), palette.data());

        const Point3 bindVertex{0.f, 0.f, 0.f};
        const Point3 skinned = transformPoint(palette[0], bindVertex);
        if (!approx(skinned.x, 1.f) || !approx(skinned.y, 0.f) || !approx(skinned.z, 0.f)) {
            return 4;
        }
    }

    {
        std::array<std::uint16_t, 1> parents{kInvalidParent};
        std::array<Mat4, 1> local{Mat4::identity()};
        std::array<Mat4, 1> global{};
        computeGlobalPose(1, parents.data(), local.data(), global.data());

        std::array<Mat4, 1> invBind{Mat4::translation(Vec3{-2.f, 0.f, 0.f})};
        std::array<Mat4, 1> palette{};
        computeMatrixPalette(1, global.data(), invBind.data(), palette.data());

        const Point3 bindVertex{2.f, 0.f, 0.f};
        const Point3 skinned = transformPoint(palette[0], bindVertex);
        if (!approx(skinned.x, 0.f) || !approx(skinned.y, 0.f) || !approx(skinned.z, 0.f)) {
            return 5;
        }
    }

    return 0;
}
