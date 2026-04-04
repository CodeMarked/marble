#include "animation/Constraints.hpp"

#include <cmath>

namespace {

bool approx(float a, float b, float eps = 1e-4f) {
    return a - b <= eps && b - a <= eps;
}

float dist(marble::math::Point3 a, marble::math::Point3 b) {
    return marble::math::length(b - a);
}

} // namespace

int main() {
    using marble::animation::clampReachTarget;
    using marble::animation::closestPointOnSegment;
    using marble::animation::solveTwoBoneMidJoint;
    using marble::math::Point3;
    using marble::math::Vec3;

    {
        const Point3 c = closestPointOnSegment({0.f, 0.f, 0.f}, {2.f, 0.f, 0.f}, {1.f, 1.f, 0.f});
        if (!approx(c.x, 1.f) || !approx(c.y, 0.f) || !approx(c.z, 0.f)) {
            return 1;
        }
    }

    {
        const Point3 c = closestPointOnSegment({0.f, 0.f, 0.f}, {2.f, 0.f, 0.f}, {-1.f, 0.f, 0.f});
        if (!approx(c.x, 0.f) || !approx(c.y, 0.f)) {
            return 2;
        }
    }

    {
        const Point3 g = clampReachTarget({0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, 3.f);
        if (!approx(g.x, 3.f) || !approx(g.y, 0.f) || !approx(g.z, 0.f)) {
            return 3;
        }
    }

    {
        const Point3 g = clampReachTarget({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, 5.f);
        if (!approx(g.x, 1.f)) {
            return 4;
        }
    }

    {
        Point3 mid{};
        Point3 adj{};
        const bool ok = solveTwoBoneMidJoint(
            {0.f, 0.f, 0.f},
            {2.f, 0.f, 0.f},
            Vec3{0.f, 1.f, 0.f},
            1.f,
            1.f,
            mid,
            &adj
        );
        if (!ok) {
            return 5;
        }
        if (!approx(dist({0.f, 0.f, 0.f}, mid), 1.f) || !approx(dist(mid, adj), 1.f)) {
            return 6;
        }
        if (!approx(adj.x, 2.f) || !approx(adj.y, 0.f) || !approx(adj.z, 0.f)) {
            return 7;
        }
    }

    {
        Point3 mid{};
        Point3 adj{};
        const bool ok = solveTwoBoneMidJoint(
            {0.f, 0.f, 0.f},
            {5.f, 0.f, 0.f},
            Vec3{0.f, 1.f, 0.f},
            1.f,
            1.f,
            mid,
            &adj
        );
        if (!ok) {
            return 8;
        }
        if (!approx(dist({0.f, 0.f, 0.f}, mid), 1.f) || !approx(dist(mid, adj), 1.f)) {
            return 9;
        }
    }

    Point3 reject{};
    if (solveTwoBoneMidJoint({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, 0.f, 1.f, reject, nullptr)) {
        return 10;
    }
    if (solveTwoBoneMidJoint({0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, 1.f, 1.f, reject, nullptr)) {
        return 11;
    }

    return 0;
}
