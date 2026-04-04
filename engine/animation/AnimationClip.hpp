#pragma once

#include "math/Mat4.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace marble::animation {

/// Per-component linear blend of two matrices. Correct for pure translations; **not** a rigid-body
/// rotation interpolant (use quaternion slerp + separate scale when rotations matter; deferred).
[[nodiscard]] inline math::Mat4 lerpMat4Elements(math::Mat4 const& a, math::Mat4 const& b, float t) noexcept {
    math::Mat4 r{};
    for (int i = 0; i < 16; ++i) {
        r.m[i] = a.m[i] + (b.m[i] - a.m[i]) * t;
    }
    return r;
}

/// Sample uniformly spaced keyframes into a local pose.
///
/// Layout: `keyframes[keyIndex * jointCount + jointIndex]` — for each key, all joint locals are stored
/// contiguously (book Ch.12 clip / channel style, flattened for the baseline).
///
/// - `keyframeCount == 1`: copies the first key to `localOut`.
/// - `durationSeconds <= 0` (with 2+ keys): uses the first key only (no extrapolation).
/// - `timeSeconds` is clamped to `[0, durationSeconds]` when `durationSeconds > 0`.
inline void sampleClipLocalPoses(
    float timeSeconds,
    float durationSeconds,
    std::uint16_t jointCount,
    std::uint16_t keyframeCount,
    math::Mat4 const* keyframes,
    math::Mat4* localOut
) noexcept {
    if (jointCount == 0u || keyframeCount == 0u || keyframes == nullptr || localOut == nullptr) {
        return;
    }
    if (keyframeCount == 1u) {
        for (std::uint16_t j = 0; j < jointCount; ++j) {
            localOut[j] = keyframes[j];
        }
        return;
    }
    if (durationSeconds <= 0.f) {
        for (std::uint16_t j = 0; j < jointCount; ++j) {
            localOut[j] = keyframes[j];
        }
        return;
    }

    float t = timeSeconds;
    if (t < 0.f) {
        t = 0.f;
    }
    if (t > durationSeconds) {
        t = durationSeconds;
    }

    const float span = static_cast<float>(keyframeCount - 1u);
    float u = (t / durationSeconds) * span;
    if (u < 0.f) {
        u = 0.f;
    }
    if (u > span) {
        u = span;
    }
    const float k0f = std::floor(u);
    float f = u - k0f;
    auto k0 = static_cast<std::uint16_t>(k0f);
    if (k0 >= keyframeCount - 1u) {
        k0 = keyframeCount - 1u;
        f = 0.f;
    }
    const auto k1 = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(static_cast<std::uint32_t>(k0) + 1u, static_cast<std::uint32_t>(keyframeCount - 1u))
    );

    for (std::uint16_t j = 0; j < jointCount; ++j) {
        const math::Mat4& a = keyframes[static_cast<std::size_t>(k0) * jointCount + j];
        const math::Mat4& b = keyframes[static_cast<std::size_t>(k1) * jointCount + j];
        localOut[j] = lerpMat4Elements(a, b, f);
    }
}

} // namespace marble::animation
