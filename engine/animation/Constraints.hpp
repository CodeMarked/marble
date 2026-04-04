#pragma once

#include "math/Vec3.hpp"

#include <algorithm>
#include <cmath>

namespace marble::animation {

/// Closest point on segment `a`–`b` to `p` (book-style geometric constraint primitive).
[[nodiscard]] inline math::Point3 closestPointOnSegment(math::Point3 a, math::Point3 b, math::Point3 p) noexcept {
    const math::Vec3 ab = b - a;
    const float den = math::lengthSquared(ab);
    if (den <= 0.f) {
        return a;
    }
    float t = math::dot(p - a, ab) / den;
    t = std::clamp(t, 0.f, 1.f);
    return a + ab * t;
}

/// Clamp an effector goal onto the sphere of radius `maxReach` around `root` (length limit / ball socket reach).
[[nodiscard]] inline math::Point3 clampReachTarget(math::Point3 root, math::Point3 desiredEnd, float maxReach) noexcept {
    if (maxReach <= 0.f) {
        return root;
    }
    const math::Vec3 v = desiredEnd - root;
    const float d = math::length(v);
    if (d <= maxReach || d <= 1e-12f) {
        return desiredEnd;
    }
    return root + math::normalize(v) * maxReach;
}

/// Two-bone IK: compute middle joint position so `|mid-root| == upperLen` and `|target-mid| == lowerLen`
/// after projecting `target` onto the reachable annulus. The elbow lies in the plane spanned by
/// `(target-root)` and `bendHint` (pole vector, e.g. character up or shoulder “out”).
///
/// Returns `false` if bone lengths are non-positive or direction from root to target degenerates.
[[nodiscard]] inline bool solveTwoBoneMidJoint(
    math::Point3 root,
    math::Point3 target,
    math::Vec3 bendHint,
    float upperLen,
    float lowerLen,
    math::Point3& outMid,
    math::Point3* outAdjustedTarget = nullptr
) noexcept {
    if (upperLen <= 0.f || lowerLen <= 0.f) {
        return false;
    }

    const float sumLen = upperLen + lowerLen;
    const float diffLen = std::fabs(upperLen - lowerLen);
    const float dMin = diffLen + 1e-4f;
    const float dMax = sumLen;
    if (dMin > dMax) {
        return false;
    }

    math::Vec3 toFull = target - root;
    const float dRaw = math::length(toFull);
    if (dRaw <= 1e-12f) {
        return false;
    }

    const math::Vec3 dir = math::normalize(toFull);
    const float d = std::clamp(dRaw, dMin, dMax);
    const math::Point3 tAdj = root + dir * d;
    if (outAdjustedTarget != nullptr) {
        *outAdjustedTarget = tAdj;
    }

    const math::Vec3 fwd = dir;
    math::Vec3 n = math::cross(fwd, bendHint);
    const float nLenSq = math::lengthSquared(n);
    if (nLenSq <= 1e-16f) {
        n = math::cross(fwd, math::Vec3::unitY());
    }
    n = math::normalize(n);
    const math::Vec3 perp = math::cross(n, fwd);

    const float aAlong = (upperLen * upperLen - lowerLen * lowerLen + d * d) / (2.f * d);
    const float hSq = upperLen * upperLen - aAlong * aAlong;
    const float h = hSq > 0.f ? std::sqrt(hSq) : 0.f;
    const float sign = math::dot(n, bendHint) >= 0.f ? 1.f : -1.f;

    outMid = root + fwd * aAlong + perp * (sign * h);
    return true;
}

} // namespace marble::animation
