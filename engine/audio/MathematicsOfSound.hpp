#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace marble::audio {

inline constexpr float kTwoPi = 6.283185307179586476925286766559f;

/// Highest frequency that can be represented without aliasing at `sampleRateHz` (Hz). Zero if invalid.
[[nodiscard]] inline float nyquistFrequencyHz(float sampleRateHz) noexcept {
    if (sampleRateHz <= 0.f) {
        return 0.f;
    }
    return 0.5f * sampleRateHz;
}

/// ω = 2πf (rad/s). Zero if `frequencyHz` is not positive.
[[nodiscard]] inline float angularFrequencyRadiansPerSecond(float frequencyHz) noexcept {
    if (frequencyHz <= 0.f) {
        return 0.f;
    }
    return kTwoPi * frequencyHz;
}

/// Advance sinusoid phase: φ' = φ + ω Δt.
[[nodiscard]] inline float advancePhaseRadians(float phaseRadians, float frequencyHz, float deltaSeconds) noexcept {
    return phaseRadians + angularFrequencyRadiansPerSecond(frequencyHz) * deltaSeconds;
}

/// Wrap phase to [0, 2π).
[[nodiscard]] inline float wrapPhaseTwoPi(float phaseRadians) noexcept {
    float p = std::fmod(phaseRadians, kTwoPi);
    if (p < 0.f) {
        p += kTwoPi;
    }
    return p;
}

/// One sample of a unit-amplitude sine at phase φ.
[[nodiscard]] inline float monoSineSample(float phaseRadians) noexcept {
    return std::sin(phaseRadians);
}

/// Duration (seconds) for `sampleCount` contiguous samples. Zero if `sampleRateHz` is not positive.
[[nodiscard]] inline float durationSecondsForSampleCount(float sampleCount, float sampleRateHz) noexcept {
    if (sampleRateHz <= 0.f || sampleCount < 0.f) {
        return 0.f;
    }
    return sampleCount / sampleRateHz;
}

/// Smallest integer sample count ≥ `durationSeconds * sampleRateHz`. Zero if inputs not positive.
[[nodiscard]] inline std::uint32_t sampleCountCeilForDuration(float durationSeconds, float sampleRateHz) noexcept {
    if (durationSeconds <= 0.f || sampleRateHz <= 0.f) {
        return 0u;
    }
    const double n = std::ceil(static_cast<double>(durationSeconds) * static_cast<double>(sampleRateHz));
    if (n <= 0.0) {
        return 0u;
    }
    if (n >= static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return static_cast<std::uint32_t>(n);
}

/// Linear blend of two samples (e.g. fractional read position). `t` should lie in [0, 1].
[[nodiscard]] inline float linearInterpolateSamples(float sampleA, float sampleB, float t) noexcept {
    return sampleA + t * (sampleB - sampleA);
}

/// Clamp float PCM in [-1, 1] to 16-bit PCM (symmetric scaling by 32768).
[[nodiscard]] inline std::int16_t floatSampleToInt16(float normalized) noexcept {
    double x = static_cast<double>(normalized) * 32768.0;
    if (x > 32767.0) {
        x = 32767.0;
    }
    if (x < -32768.0) {
        x = -32768.0;
    }
    return static_cast<std::int16_t>(std::lround(x));
}

/// Map 16-bit PCM back to approximately [-1, 1].
[[nodiscard]] inline float int16ToFloatSample(std::int16_t sample) noexcept {
    return static_cast<float>(static_cast<double>(sample) / 32768.0);
}

} // namespace marble::audio
