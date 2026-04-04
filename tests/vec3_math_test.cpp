#include "math/Vec3.hpp"

#include <cmath>
#include <cstdlib>

namespace {

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

static_assert(marble::math::dot(marble::math::Vec3::unitX(), marble::math::Vec3::unitY()) == 0.f);
static_assert(marble::math::lengthSquared(marble::math::Vec3::unitZ()) == 1.f);

static constexpr marble::math::Vec3 kCrossIJ =
    marble::math::cross(marble::math::Vec3::unitX(), marble::math::Vec3::unitY());
static_assert(kCrossIJ.x == 0.f && kCrossIJ.y == 0.f && kCrossIJ.z == 1.f);

static constexpr marble::math::Vec3 kLerpMid =
    marble::math::lerp(marble::math::Vec3::zero(), marble::math::Vec3::unitY(), 0.5f);
static_assert(kLerpMid.x == 0.f && kLerpMid.y == 0.5f && kLerpMid.z == 0.f);

int main() {
    using marble::math::Vec3;

    const Vec3 a{3.f, 4.f, 0.f};
    if (!approx(marble::math::length(a), 5.f)) {
        return 1;
    }
    if (!approx(marble::math::lengthSquared(a), 25.f)) {
        return 2;
    }

    const Vec3 n = marble::math::normalize(a);
    if (!approx(n.x, 0.6f) || !approx(n.y, 0.8f) || !approx(n.z, 0.f)) {
        return 3;
    }

    const Vec3 z = marble::math::cross(Vec3::unitX(), Vec3::unitY());
    if (!approx(z.x, 0.f) || !approx(z.y, 0.f) || !approx(z.z, 1.f)) {
        return 4;
    }

    // Sphere centers distance (book §5.2.4.4): compare squared distance to radii sum squared.
    const marble::math::Point3 c1{0.f, 0.f, 0.f};
    const marble::math::Point3 c2{3.f, 4.f, 0.f};
    const Vec3 d = c2 - c1;
    if (marble::math::lengthSquared(d) != 25.f) {
        return 5;
    }

    return 0;
}
