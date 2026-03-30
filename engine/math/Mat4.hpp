#pragma once

#include "math/Vec3.hpp"

#include <cmath>
#include <cstddef>

namespace marble::math {

/// Homogeneous column vector for matrix multiply (book §5.3 / homogeneous coords).
struct Vec4 {
    float x{};
    float y{};
    float z{};
    float w{};
};

/// 4×4 matrix in **column-major** order: column `c` occupies `m[c * 4 + 0..3]` (row 0..3).
/// Transform applies as **column vectors**: `v' = M * v` (book §5.3 matrix–vector conventions).
struct Mat4 {
    float m[16]{};

    [[nodiscard]] static constexpr Mat4 identity() noexcept {
        Mat4 r{};
        r.m[0] = 1.f;
        r.m[5] = 1.f;
        r.m[10] = 1.f;
        r.m[15] = 1.f;
        return r;
    }

    /// Translation on **affine** column vectors (last column holds tx, ty, tz).
    [[nodiscard]] static constexpr Mat4 translation(Vec3 t) noexcept {
        Mat4 r = identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    /// Non-uniform scale about the origin (diagonal sx, sy, sz).
    [[nodiscard]] static constexpr Mat4 scaling(Vec3 s) noexcept {
        Mat4 r{};
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        r.m[15] = 1.f;
        return r;
    }

    /// Rotation about **+Y** (up), radians, **left-handed** world (ADR-0017): positive angle
    /// moves **+X** toward **+Z** (yaw). Uses `std::sin` / `std::cos` (book §5.3 rotation matrices).
    [[nodiscard]] static inline Mat4 rotationY(float radians) noexcept {
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        Mat4 r{};
        r.m[0] = c;
        r.m[2] = -s;
        r.m[5] = 1.f;
        r.m[8] = s;
        r.m[10] = c;
        r.m[15] = 1.f;
        return r;
    }
};

[[nodiscard]] constexpr float mat4Elem(Mat4 const& a, std::size_t row, std::size_t col) noexcept {
    return a.m[col * 4 + row];
}

/// Matrix product `A * B` (applies **B** first, then **A**: `(A*B)*v = A*(B*v)`).
[[nodiscard]] inline Mat4 operator*(Mat4 const& a, Mat4 const& b) noexcept {
    Mat4 c{};
    for (std::size_t j = 0; j < 4; ++j) {
        for (std::size_t i = 0; i < 4; ++i) {
            float s = 0.f;
            for (std::size_t k = 0; k < 4; ++k) {
                s += mat4Elem(a, i, k) * mat4Elem(b, k, j);
            }
            c.m[j * 4 + i] = s;
        }
    }
    return c;
}

[[nodiscard]] constexpr Vec4 operator*(Mat4 const& m, Vec4 const& v) noexcept {
    return {
        mat4Elem(m, 0, 0) * v.x + mat4Elem(m, 0, 1) * v.y + mat4Elem(m, 0, 2) * v.z + mat4Elem(m, 0, 3) * v.w,
        mat4Elem(m, 1, 0) * v.x + mat4Elem(m, 1, 1) * v.y + mat4Elem(m, 1, 2) * v.z + mat4Elem(m, 1, 3) * v.w,
        mat4Elem(m, 2, 0) * v.x + mat4Elem(m, 2, 1) * v.y + mat4Elem(m, 2, 2) * v.z + mat4Elem(m, 2, 3) * v.w,
        mat4Elem(m, 3, 0) * v.x + mat4Elem(m, 3, 1) * v.y + mat4Elem(m, 3, 2) * v.z + mat4Elem(m, 3, 3) * v.w,
    };
}

/// Point transform: uses `w = 1`. If `w'` is not ~1, performs perspective divide (projection matrices).
[[nodiscard]] inline Point3 transformPoint(Mat4 const& m, Point3 p) noexcept {
    const Vec4 v{m * Vec4{p.x, p.y, p.z, 1.f}};
    if (std::fabs(v.w - 1.f) > 1e-5f && std::fabs(v.w) > 1e-12f) {
        const float inv = 1.f / v.w;
        return {v.x * inv, v.y * inv, v.z * inv};
    }
    return {v.x, v.y, v.z};
}

/// Direction / offset: uses `w = 0` (ignores translation; linear part only). Not re-normalized.
[[nodiscard]] constexpr Vec3 transformDirection(Mat4 const& m, Vec3 d) noexcept {
    const Vec4 v{m * Vec4{d.x, d.y, d.z, 0.f}};
    return {v.x, v.y, v.z};
}

} // namespace marble::math
