#pragma once

#include "animation/AnimationClip.hpp"
#include "math/Mat4.hpp"
#include "math/Vec3.hpp"

#include <algorithm>
#include <cstdint>

namespace marble::animation {

/// Clamp a cross-fade / layer weight to `[0, 1]`.
[[nodiscard]] inline float blendWeightClamp(float alpha) noexcept {
    return std::clamp(alpha, 0.f, 1.f);
}

/// Per-joint blend of two local poses using `lerpMat4Elements` (same rotation caveats as clip sampling).
inline void blendLocalPoses(
    std::uint16_t jointCount,
    float alpha,
    math::Mat4 const* poseA,
    math::Mat4 const* poseB,
    math::Mat4* outPose
) noexcept {
    if (jointCount == 0u || poseA == nullptr || poseB == nullptr || outPose == nullptr) {
        return;
    }
    const float w = blendWeightClamp(alpha);
    for (std::uint16_t j = 0; j < jointCount; ++j) {
        outPose[j] = lerpMat4Elements(poseA[j], poseB[j], w);
    }
}

/// Additive translation on one joint's **local** affine transform (column-major translation in `m[12..14]`).
inline void addLocalJointTranslation(
    std::uint16_t jointIndex,
    math::Vec3 delta,
    std::uint16_t jointCount,
    math::Mat4* localPose
) noexcept {
    if (localPose == nullptr || jointIndex >= jointCount) {
        return;
    }
    math::Mat4& m = localPose[jointIndex];
    m.m[12] += delta.x;
    m.m[13] += delta.y;
    m.m[14] += delta.z;
}

/// Left-multiply one joint's local matrix (procedural correction before FK: `local' = pre * local`).
inline void preMultiplyLocalJoint(
    std::uint16_t jointIndex,
    math::Mat4 const& pre,
    std::uint16_t jointCount,
    math::Mat4* localPose
) noexcept {
    if (localPose == nullptr || jointIndex >= jointCount) {
        return;
    }
    localPose[jointIndex] = pre * localPose[jointIndex];
}

} // namespace marble::animation
