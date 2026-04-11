#pragma once

#include "math/Vec3.hpp"

#include <cmath>

namespace marble::garden {

/// Integrate `position += linearVelocity * dt` when inputs are finite and `dt > 0`.
[[nodiscard]] inline bool integrateKinematicPositionTick(
    math::Vec3& position,
    math::Vec3 const& linearVelocity,
    float deltaSeconds) noexcept {
    if (!(deltaSeconds > 0.f) || !std::isfinite(deltaSeconds)) {
        return false;
    }
    if (!std::isfinite(linearVelocity.x) || !std::isfinite(linearVelocity.y) ||
        !std::isfinite(linearVelocity.z)) {
        return false;
    }
    position = position + linearVelocity * deltaSeconds;
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

struct RemoteAuthorityReconcileResult {
    bool hardSnapped{};
};

/// Snap `position` to `authorityPosition` only when separation exceeds `snapThresholdM`.
/// No per-frame “soft” pull — that fought render-time prediction and read as rubber-band jitter.
[[nodiscard]] inline RemoteAuthorityReconcileResult reconcileEmergencyPositionSnap(
    math::Vec3& position,
    math::Vec3 const& authorityPosition,
    float snapThresholdM) noexcept {
    RemoteAuthorityReconcileResult out{};
    math::Vec3 const delta = authorityPosition - position;
    float const distSq = math::lengthSquared(delta);
    float const th2 = snapThresholdM * snapThresholdM;
    if (!std::isfinite(distSq) || !(distSq > th2)) {
        return out;
    }
    position = authorityPosition;
    out.hardSnapped = true;
    return out;
}

} // namespace marble::garden
