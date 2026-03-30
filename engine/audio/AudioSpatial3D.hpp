#pragma once

#include "audio/PhysicsOfSound.hpp"
#include "math/Vec3.hpp"

#include <cmath>

namespace marble::audio {

/// Listener pose in world space (ADR-0017: left-handed, +Y up, +Z forward).
struct Listener3D {
    math::Vec3 position{};
    math::Vec3 forward{0.f, 0.f, 1.f};
    /// Resolves `right` via cross products; orthonormal `up` is derived in [`buildListenerBasis`].
    math::Vec3 worldUpHint{0.f, 1.f, 0.f};
};

/// Orthonormal listener frame at [`position`]: `forward` × `right` = `up`.
struct ListenerBasis {
    math::Vec3 position{};
    math::Vec3 forward{};
    math::Vec3 right{};
    math::Vec3 up{};
    bool valid{};
};

/// Builds a listener orthonormal basis. Fails if `forward` or `worldUpHint` are degenerate or parallel to each other.
[[nodiscard]] inline ListenerBasis buildListenerBasis(Listener3D const& listener) noexcept {
    constexpr float kEpsSq = 1e-12f;
    const math::Vec3 f = math::normalize(listener.forward);
    if (math::lengthSquared(f) < kEpsSq) {
        return {{}, {}, {}, {}, false};
    }
    const math::Vec3 r = math::normalize(math::cross(listener.worldUpHint, f));
    if (math::lengthSquared(r) < kEpsSq) {
        return {{}, {}, {}, {}, false};
    }
    const math::Vec3 u = math::normalize(math::cross(f, r));
    return {listener.position, f, r, u, true};
}

/// Horizontal-plane stereo pan in [-1, 1]: -1 full left, +1 full right. Elevation is ignored (offset projected onto the horizontal plane spanned by `forward` and `right`).
[[nodiscard]] inline float stereoPan11(ListenerBasis const& basis, math::Vec3 sourceWorldPosition) noexcept {
    constexpr float kEpsSq = 1e-12f;
    if (!basis.valid) {
        return 0.f;
    }
    const math::Vec3 offset = sourceWorldPosition - basis.position;
    const float alongUp = math::dot(offset, basis.up);
    const math::Vec3 horizontal = offset - basis.up * alongUp;
    const float h2 = math::lengthSquared(horizontal);
    if (h2 < kEpsSq) {
        return 0.f;
    }
    const math::Vec3 hDir = horizontal * (1.f / std::sqrt(h2));
    float p = math::dot(hDir, basis.right);
    if (p < -1.f) {
        p = -1.f;
    } else if (p > 1.f) {
        p = 1.f;
    }
    return p;
}

/// Piecewise-linear gain: full level at `minDistanceMeters`, silent at `maxDistanceMeters`, linear between.
[[nodiscard]] inline float distanceAttenuationLinear(float distanceMeters, float minDistanceMeters,
                                                     float maxDistanceMeters) noexcept {
    if (distanceMeters <= 0.f || maxDistanceMeters <= minDistanceMeters) {
        return 0.f;
    }
    if (distanceMeters <= minDistanceMeters) {
        return 1.f;
    }
    if (distanceMeters >= maxDistanceMeters) {
        return 0.f;
    }
    return (maxDistanceMeters - distanceMeters) / (maxDistanceMeters - minDistanceMeters);
}

/// 1/r pressure-style falloff vs `referenceDistanceMeters` (delegates to `pointSourcePressureScale`).
[[nodiscard]] inline float distanceAttenuationInverseDistance(float distanceMeters,
                                                              float referenceDistanceMeters = 1.f) noexcept {
    return pointSourcePressureScale(distanceMeters, referenceDistanceMeters);
}

/// Linear pan law: `outLeft` / `outRight` are non-normalized gains (sum ≈ `distanceGain` when `pan11` ∈ [-1, 1]).
inline void stereoSpeakerGainsLinear(float pan11, float distanceGain, float& outLeft, float& outRight) noexcept {
    float p = pan11;
    if (p < -1.f) {
        p = -1.f;
    } else if (p > 1.f) {
        p = 1.f;
    }
    outLeft = distanceGain * 0.5f * (1.f - p);
    outRight = distanceGain * 0.5f * (1.f + p);
}

/// Component of source velocity toward the listener (m/s), positive when the source moves toward the listener.
[[nodiscard]] inline float radialVelocitySourceTowardListener(math::Vec3 listenerWorldPosition,
                                                              math::Vec3 sourceWorldPosition,
                                                              math::Vec3 sourceVelocityWorldMps) noexcept {
    constexpr float kEpsSq = 1e-12f;
    const math::Vec3 toListener = listenerWorldPosition - sourceWorldPosition;
    const float lenSq = math::lengthSquared(toListener);
    if (lenSq < kEpsSq) {
        return 0.f;
    }
    const float invLen = 1.f / std::sqrt(lenSq);
    const math::Vec3 dirToListener = toListener * invLen;
    return math::dot(sourceVelocityWorldMps, dirToListener);
}

/// Stationary listener, subsonic source: scale ≈ c / (c − v∥) with v∥ toward listener (textbook Doppler sketch).
[[nodiscard]] inline float dopplerPitchScale(float speedOfSoundMps,
                                            float radialVelocitySourceTowardListenerMps) noexcept {
    if (speedOfSoundMps <= 1e-6f) {
        return 1.f;
    }
    const float denom = speedOfSoundMps - radialVelocitySourceTowardListenerMps;
    if (denom <= 1e-3f) {
        return 1.f;
    }
    return speedOfSoundMps / denom;
}

} // namespace marble::audio
