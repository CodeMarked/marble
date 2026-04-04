#pragma once

#include "math/Geometry.hpp"

#include <cstdint>

namespace marble::physics {

/// Sphere–sphere overlap (narrow-phase primitive; book Ch.13 collision queries).
[[nodiscard]] constexpr bool intersects(math::Sphere const& a, math::Sphere const& b) noexcept {
    const math::Vec3 d = a.center - b.center;
    const float r = a.radius + b.radius;
    return math::lengthSquared(d) <= r * r;
}

/// AABB–AABB overlap (typical broad-phase proxy test).
[[nodiscard]] constexpr bool intersects(math::Aabb const& a, math::Aabb const& b) noexcept {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

/// Bit masks for **middleware-style** pairing: each body publishes what layers it belongs to and
/// which layers it accepts hits from; both directions must agree (symmetric filter).
struct CollisionFilter {
    std::uint32_t membershipLayers{};
    std::uint32_t collideAgainstMask{};
};

[[nodiscard]] constexpr bool filtersAllow(CollisionFilter const& x, CollisionFilter const& y) noexcept {
    return (x.membershipLayers & y.collideAgainstMask) != 0u &&
           (y.membershipLayers & x.collideAgainstMask) != 0u;
}

} // namespace marble::physics
