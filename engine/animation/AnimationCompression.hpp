#pragma once

#include "math/Vec3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace marble::animation {

/// **Runtime animation pipeline** (book §12.9, Marble ordering): decompress/sample clips → blend
/// local poses → local post-process (`AnimationBlend.hpp`) → forward kinematics (`SkeletonPose.hpp`) →
/// skinning palette (`Skinning.hpp`). Authoring/export is the inverse problem (DCC → packed assets).

/// Linear quantization of a translation component (meters in world space) to `int16`.
/// `unitsPerMeter` is the number of integer steps per meter before clamping (e.g. `4096.f` ≈ 0.24 mm steps).
[[nodiscard]] inline std::int16_t quantizeTranslation(float meters, float unitsPerMeter) noexcept {
    const float scaled = meters * unitsPerMeter;
    const float clamped = std::clamp(scaled, -32768.f, 32767.f);
    return static_cast<std::int16_t>(std::lrintf(clamped));
}

[[nodiscard]] inline float dequantizeTranslation(std::int16_t stored, float unitsPerMeter) noexcept {
    if (unitsPerMeter == 0.f) {
        return 0.f;
    }
    return static_cast<float>(stored) / unitsPerMeter;
}

inline void quantizeTranslationVec3(math::Vec3 meters, float unitsPerMeter, std::int16_t* outXyz) noexcept {
    if (outXyz == nullptr) {
        return;
    }
    outXyz[0] = quantizeTranslation(meters.x, unitsPerMeter);
    outXyz[1] = quantizeTranslation(meters.y, unitsPerMeter);
    outXyz[2] = quantizeTranslation(meters.z, unitsPerMeter);
}

[[nodiscard]] inline math::Vec3 dequantizeTranslationVec3(
    std::int16_t x,
    std::int16_t y,
    std::int16_t z,
    float unitsPerMeter
) noexcept {
    return {
        dequantizeTranslation(x, unitsPerMeter),
        dequantizeTranslation(y, unitsPerMeter),
        dequantizeTranslation(z, unitsPerMeter),
    };
}

/// Normalized scalar in `[0, 1]` (e.g. key time / clip duration) packed to 16 bits.
[[nodiscard]] inline std::uint16_t quantizeUnitInterval(float u01) noexcept {
    const float u = std::clamp(u01, 0.f, 1.f);
    return static_cast<std::uint16_t>(std::lrintf(u * 65535.f));
}

[[nodiscard]] inline float dequantizeUnitInterval(std::uint16_t stored) noexcept {
    return static_cast<float>(stored) / 65535.f;
}

} // namespace marble::animation
