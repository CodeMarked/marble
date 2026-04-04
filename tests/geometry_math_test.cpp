#include "math/Geometry.hpp"

#include <cmath>

namespace {

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    using namespace marble::math;

    const Line3 line{{1.f, 2.f, 3.f}, Vec3::unitX()};
    const Point3 l = pointOnLine(line, 2.f);
    if (!approx(l.x, 3.f) || !approx(l.y, 2.f) || !approx(l.z, 3.f)) {
        return 1;
    }

    const Ray3 ray{{0.f, 0.f, 0.f}, normalize(Vec3{0.f, 1.f, 1.f})};
    const Point3 r = pointOnRay(ray, 2.f);
    if (!(r.y > 1.4f && r.z > 1.4f)) {
        return 2;
    }

    const Segment3 seg{{0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}};
    const Point3 s = pointOnSegment(seg, 0.25f);
    if (!approx(s.x, 2.5f) || !approx(s.y, 0.f) || !approx(s.z, 0.f)) {
        return 3;
    }

    const Sphere sphere{{0.f, 0.f, 0.f}, 2.f};
    if (!contains(sphere, Point3{1.f, 1.f, 0.f})) {
        return 4;
    }
    if (contains(sphere, Point3{3.f, 0.f, 0.f})) {
        return 5;
    }

    const Plane plane{Vec3::unitY(), -1.f}; // y - 1 = 0
    if (!approx(signedDistance(plane, Point3{0.f, 3.f, 0.f}), 2.f)) {
        return 6;
    }
    if (!approx(signedDistance(plane, Point3{0.f, 0.f, 0.f}), -1.f)) {
        return 7;
    }

    const Aabb box{{-1.f, -2.f, -3.f}, {1.f, 2.f, 3.f}};
    if (!contains(box, Point3{0.f, 0.f, 0.f})) {
        return 8;
    }
    if (contains(box, Point3{2.f, 0.f, 0.f})) {
        return 9;
    }

    // Cube frustum [-1,1]^3 represented as six inward-facing planes.
    const Frustum frustum{
        Plane{Vec3{1.f, 0.f, 0.f}, 1.f},   // x >= -1
        Plane{Vec3{-1.f, 0.f, 0.f}, 1.f},  // x <= 1
        Plane{Vec3{0.f, 1.f, 0.f}, 1.f},   // y >= -1
        Plane{Vec3{0.f, -1.f, 0.f}, 1.f},  // y <= 1
        Plane{Vec3{0.f, 0.f, 1.f}, 1.f},   // z >= -1
        Plane{Vec3{0.f, 0.f, -1.f}, 1.f},  // z <= 1
    };
    if (!contains(frustum, Point3{0.f, 0.f, 0.f})) {
        return 10;
    }
    if (contains(frustum, Point3{2.f, 0.f, 0.f})) {
        return 11;
    }

    return 0;
}
