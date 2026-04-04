#pragma once

#include "math/Vec3.hpp"

#include <array>

namespace marble::math {

struct Line3 {
    Point3 p0{};
    Vec3 directionUnit{};
};

struct Ray3 {
    Point3 origin{};
    Vec3 directionUnit{};
};

struct Segment3 {
    Point3 p0{};
    Point3 p1{};
};

struct Sphere {
    Point3 center{};
    float radius{};
};

/// Plane in compact [n d] form: dot(n, p) + d = 0 for points on the plane.
struct Plane {
    Vec3 normalUnit{};
    float d{};
};

struct Aabb {
    Point3 min{};
    Point3 max{};
};

using Frustum = std::array<Plane, 6>;

[[nodiscard]] constexpr Point3 pointOnLine(Line3 const& line, float t) noexcept {
    return line.p0 + line.directionUnit * t;
}

[[nodiscard]] constexpr Point3 pointOnRay(Ray3 const& ray, float t) noexcept {
    return ray.origin + ray.directionUnit * t;
}

[[nodiscard]] constexpr Point3 pointOnSegment(Segment3 const& seg, float t) noexcept {
    return seg.p0 + (seg.p1 - seg.p0) * t;
}

[[nodiscard]] constexpr bool contains(Sphere const& sphere, Point3 p) noexcept {
    const Vec3 d = p - sphere.center;
    return lengthSquared(d) <= sphere.radius * sphere.radius;
}

[[nodiscard]] constexpr float signedDistance(Plane const& plane, Point3 p) noexcept {
    return dot(plane.normalUnit, p) + plane.d;
}

[[nodiscard]] constexpr bool contains(Aabb const& box, Point3 p) noexcept {
    return p.x >= box.min.x && p.x <= box.max.x && p.y >= box.min.y && p.y <= box.max.y &&
           p.z >= box.min.z && p.z <= box.max.z;
}

[[nodiscard]] constexpr bool contains(Frustum const& frustum, Point3 p) noexcept {
    for (Plane const& plane : frustum) {
        if (signedDistance(plane, p) < 0.f) {
            return false;
        }
    }
    return true;
}

} // namespace marble::math
