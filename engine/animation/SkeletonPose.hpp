#pragma once

#include "math/Mat4.hpp"

#include <cstdint>

namespace marble::animation {

/// Parent index for the skeleton root (joint 0).
inline constexpr std::uint16_t kInvalidParent = 0xFFFFu;

/// Forward kinematics: `globalPoseOut[0] = localPose[0]`; for `i > 0`,
/// `globalPoseOut[i] = globalPoseOut[parentIndices[i]] * localPose[i]`.
///
/// Expected layout (book Ch.12 pose / skeleton conventions): `parentIndices[i] < i` for all
/// non-root joints so each parent is computed before its children. Joint `0` is the single root
/// with `parentIndices[0] == kInvalidParent`.
[[nodiscard]] inline bool skeletonParentsWellOrdered(
    std::uint16_t jointCount,
    std::uint16_t const* parentIndices
) noexcept {
    if (jointCount == 0u || parentIndices == nullptr) {
        return false;
    }
    if (parentIndices[0] != kInvalidParent) {
        return false;
    }
    for (std::uint16_t i = 1; i < jointCount; ++i) {
        const std::uint16_t p = parentIndices[i];
        if (p == kInvalidParent) {
            return false;
        }
        if (p >= i) {
            return false;
        }
    }
    return true;
}

inline void computeGlobalPose(
    std::uint16_t jointCount,
    std::uint16_t const* parentIndices,
    math::Mat4 const* localPose,
    math::Mat4* globalPoseOut
) noexcept {
    if (jointCount == 0u || parentIndices == nullptr || localPose == nullptr || globalPoseOut == nullptr) {
        return;
    }
    globalPoseOut[0] = localPose[0];
    for (std::uint16_t i = 1; i < jointCount; ++i) {
        const std::uint16_t p = parentIndices[i];
        globalPoseOut[i] = globalPoseOut[p] * localPose[i];
    }
}

} // namespace marble::animation
