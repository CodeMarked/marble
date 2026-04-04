#include "math/Mat4.hpp"

#include <cmath>
#include <cstdlib>

namespace {

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

static_assert(marble::math::mat4Elem(marble::math::Mat4::identity(), 0, 0) == 1.f);
static_assert(marble::math::mat4Elem(marble::math::Mat4::identity(), 1, 1) == 1.f);
static_assert(marble::math::mat4Elem(marble::math::Mat4::translation({2.f, 3.f, 4.f}), 0, 3) == 2.f);
static_assert(marble::math::mat4Elem(marble::math::Mat4::translation({2.f, 3.f, 4.f}), 1, 3) == 3.f);
static_assert(marble::math::mat4Elem(marble::math::Mat4::translation({2.f, 3.f, 4.f}), 2, 3) == 4.f);

int main() {
    using marble::math::Mat4;
    using marble::math::Point3;
    using marble::math::Vec3;

    const Point3 origin{0.f, 0.f, 0.f};
    const Point3 t = marble::math::transformPoint(Mat4::translation({1.f, 2.f, 3.f}), origin);
    if (!approx(t.x, 1.f) || !approx(t.y, 2.f) || !approx(t.z, 3.f)) {
        return 1;
    }

    const Point3 p = marble::math::transformPoint(Mat4::identity(), Point3{5.f, -1.f, 2.f});
    if (!approx(p.x, 5.f) || !approx(p.y, -1.f) || !approx(p.z, 2.f)) {
        return 2;
    }

    // scale(2,3,4) * (1,1,1) -> (2,3,4)
    const Point3 s = marble::math::transformPoint(Mat4::scaling({2.f, 3.f, 4.f}), Point3{1.f, 1.f, 1.f});
    if (!approx(s.x, 2.f) || !approx(s.y, 3.f) || !approx(s.z, 4.f)) {
        return 3;
    }

    // Direction ignores translation: T * (d,0) should not add t.
    const Mat4 tr = Mat4::translation(Vec3{10.f, 20.f, 30.f});
    const Vec3 td = marble::math::transformDirection(tr, Vec3::unitX());
    if (!approx(td.x, 1.f) || !approx(td.y, 0.f) || !approx(td.z, 0.f)) {
        return 4;
    }

    // rotationY(π/2): +X -> (0,0,-1) in left-handed +Y-up (see ADR-0018).
    const float halfPi = 1.57079632679f;
    const Point3 ry = marble::math::transformPoint(Mat4::rotationY(halfPi), Point3{1.f, 0.f, 0.f});
    if (!approx(ry.x, 0.f) || !approx(ry.y, 0.f) || !approx(ry.z, -1.f)) {
        return 5;
    }

    // (A*B)*v = A*(B*v) for translation then rotation on a point.
    const Mat4 m = Mat4::rotationY(halfPi) * Mat4::translation(Vec3{1.f, 0.f, 0.f});
    const Point3 q = marble::math::transformPoint(m, origin);
    if (!approx(q.x, 0.f) || !approx(q.y, 0.f) || !approx(q.z, -1.f)) {
        return 6;
    }

    return 0;
}
