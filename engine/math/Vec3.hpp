#pragma once

#include <cmath>
#include <cstddef>

namespace marble::math {

/// Three-component vector for positions, directions, and offsets (book §5.2).
/// Coordinate convention is documented in ADR-0017 (left-handed, +Y up, +Z forward).
struct Vec3 {
    float x{};
    float y{};
    float z{};

    static constexpr Vec3 zero() noexcept { return {}; }
    static constexpr Vec3 unitX() noexcept { return {1.f, 0.f, 0.f}; }
    static constexpr Vec3 unitY() noexcept { return {0.f, 1.f, 0.f}; }
    static constexpr Vec3 unitZ() noexcept { return {0.f, 0.f, 1.f}; }

    constexpr Vec3 operator+(Vec3 o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }
    constexpr Vec3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
};

constexpr Vec3 operator*(float s, Vec3 v) noexcept {
    return v * s;
}

[[nodiscard]] constexpr float dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] constexpr float lengthSquared(Vec3 v) noexcept {
    return dot(v, v);
}

[[nodiscard]] inline float length(Vec3 v) noexcept {
    return std::sqrt(lengthSquared(v));
}

/// Standard component formula (book eq. near §5.2.4.8). Visualization uses the left-hand rule when
/// the engine convention is left-handed (book §5.2.4.8).
[[nodiscard]] constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/// Unit vector in the direction of `v`, or zero if length is zero (book §5.2.4.5).
[[nodiscard]] inline Vec3 normalize(Vec3 v) noexcept {
    const float sq = lengthSquared(v);
    if (sq <= 0.f) {
        return {};
    }
    return v * (1.f / std::sqrt(sq));
}

/// Linear interpolation (book §5.2.5). `t` typically in [0, 1].
[[nodiscard]] constexpr Vec3 lerp(Vec3 a, Vec3 b, float t) noexcept {
    return a * (1.f - t) + b * t;
}

/// Same storage as `Vec3`; points are absolute, direction vectors relative (book §5.2.3).
using Point3 = Vec3;

} // namespace marble::math
