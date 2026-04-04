#pragma once

#include "math/Geometry.hpp"
#include "physics/CollisionMiddleware.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marble::physics {

/// Axis-aligned proxy for naive broad-phase pair tests (book Ch.13 broad-phase baseline).
struct BroadphaseProxy {
    std::uint32_t id{};
    math::Aabb bounds{};
};

/// All unordered overlapping pairs `(i, j), i < j`, capped at `maxPairs`. Returns count written.
inline std::size_t collectAabbOverlapPairs(
    BroadphaseProxy const* proxies,
    std::size_t count,
    std::uint32_t* outA,
    std::uint32_t* outB,
    std::size_t maxPairs
) noexcept {
    if (proxies == nullptr || outA == nullptr || outB == nullptr || maxPairs == 0u) {
        return 0u;
    }
    std::size_t written = 0u;
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            if (intersects(proxies[i].bounds, proxies[j].bounds)) {
                if (written >= maxPairs) {
                    return written;
                }
                outA[written] = proxies[i].id;
                outB[written] = proxies[j].id;
                ++written;
            }
        }
    }
    return written;
}

/// Slab clipping of an infinite line along `ray`: parametric `p = origin + t * directionUnit`.
/// On success, `tMin <= tMax` is the overlap interval (may be negative).
[[nodiscard]] inline bool rayAabbInterval(
    math::Ray3 const& ray,
    math::Aabb const& box,
    float& tMin,
    float& tMax
) noexcept {
    float t0 = -std::numeric_limits<float>::infinity();
    float t1 = std::numeric_limits<float>::infinity();

    auto clipAxis = [&](float o, float d, float bmin, float bmax) -> bool {
        constexpr float kEps = 1e-8f;
        if (std::fabs(d) < kEps) {
            if (o < bmin || o > bmax) {
                return false;
            }
            return true;
        }
        float inv = 1.f / d;
        float s0 = (bmin - o) * inv;
        float s1 = (bmax - o) * inv;
        if (s0 > s1) {
            std::swap(s0, s1);
        }
        t0 = std::max(t0, s0);
        t1 = std::min(t1, s1);
        return t0 <= t1;
    };

    if (!clipAxis(ray.origin.x, ray.directionUnit.x, box.min.x, box.max.x)) {
        return false;
    }
    if (!clipAxis(ray.origin.y, ray.directionUnit.y, box.min.y, box.max.y)) {
        return false;
    }
    if (!clipAxis(ray.origin.z, ray.directionUnit.z, box.min.z, box.max.z)) {
        return false;
    }

    tMin = t0;
    tMax = t1;
    return true;
}

/// First hit along the ray with `t >= 0`. Returns `false` if the forward ray misses the box.
[[nodiscard]] inline bool raycastAabb(math::Ray3 const& ray, math::Aabb const& box, float& tHit) noexcept {
    float t0{};
    float t1{};
    if (!rayAabbInterval(ray, box, t0, t1)) {
        return false;
    }
    const float tEnter = std::max(t0, 0.f);
    if (tEnter > t1) {
        return false;
    }
    tHit = tEnter;
    return true;
}

/// Ray versus sphere: smallest non-negative `t` along `directionUnit`, or `false` if no forward hit.
[[nodiscard]] inline bool raycastSphere(math::Ray3 const& ray, math::Sphere const& sphere, float& tHit) noexcept {
    const math::Vec3 oc = ray.origin - sphere.center;
    const float b = math::dot(oc, ray.directionUnit);
    const float c = math::dot(oc, oc) - sphere.radius * sphere.radius;
    const float disc = b * b - c;
    if (disc < 0.f) {
        return false;
    }
    const float s = std::sqrt(disc);
    float t = -b - s;
    if (t >= 0.f) {
        tHit = t;
        return true;
    }
    t = -b + s;
    if (t >= 0.f) {
        tHit = t;
        return true;
    }
    return false;
}

} // namespace marble::physics
