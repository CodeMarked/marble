#pragma once

#include "math/Mat4.hpp"

#include <cstdint>

namespace marble::animation {

/// GPU skinning palette entry: current global joint transform times inverse bind (bind-pose) transform.
/// Typical vertex deformation: sum of weighted `palette[joint] * positionInBindSpace` (book §12.5).
inline void computeMatrixPalette(
    std::uint16_t jointCount,
    math::Mat4 const* globalPose,
    math::Mat4 const* inverseBindPose,
    math::Mat4* paletteOut
) noexcept {
    if (jointCount == 0u || globalPose == nullptr || inverseBindPose == nullptr || paletteOut == nullptr) {
        return;
    }
    for (std::uint16_t i = 0; i < jointCount; ++i) {
        paletteOut[i] = globalPose[i] * inverseBindPose[i];
    }
}

} // namespace marble::animation
