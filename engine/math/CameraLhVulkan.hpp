#pragma once

#include "math/Mat4.hpp"
#include "math/Vec3.hpp"

#include <cmath>

namespace marble::math {

/// Left-handed view: column basis [right, up, -forward] for use with Vulkan-style projections (w = -z_view).
[[nodiscard]] inline Mat4 lookAtLh(Vec3 const& eye, Vec3 const& target, Vec3 const& worldUp) noexcept {
    Vec3 const f = normalize(target - eye);
    Vec3 mutUp = worldUp;
    Vec3 r = cross(mutUp, f);
    if (lengthSquared(r) < 1e-8f) {
        mutUp = Vec3::unitX();
        r = cross(mutUp, f);
    }
    r = normalize(r);
    Vec3 const u = cross(f, r);
    Mat4 m{};
    m.m[0] = r.x;
    m.m[1] = u.x;
    m.m[2] = -f.x;
    m.m[3] = 0.f;
    m.m[4] = r.y;
    m.m[5] = u.y;
    m.m[6] = -f.y;
    m.m[7] = 0.f;
    m.m[8] = r.z;
    m.m[9] = u.z;
    m.m[10] = -f.z;
    m.m[11] = 0.f;
    m.m[12] = -dot(r, eye);
    m.m[13] = -dot(u, eye);
    m.m[14] = dot(f, eye);
    m.m[15] = 1.f;
    return m;
}

/// Vulkan [0,1] depth clip, infinite far plane style column-major matrix (matches sample `perspectiveVulkan`).
[[nodiscard]] inline Mat4 perspectiveVulkan(float fovyRad, float aspect, float n, float f) noexcept {
    float const t = std::tan(fovyRad * 0.5f);
    if (t <= 1e-8f) {
        return Mat4::identity();
    }
    float const invT = 1.f / t;
    Mat4 p{};
    p.m[0] = invT / aspect;
    p.m[5] = invT;
    p.m[10] = f / (n - f);
    p.m[11] = -1.f;
    p.m[14] = (n * f) / (n - f);
    return p;
}

} // namespace marble::math
