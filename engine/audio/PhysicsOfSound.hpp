#pragma once

#include <cmath>
#include <limits>

namespace marble::audio {

/// Speed of sound in dry air near 20 °C (m/s). Common textbook/game value; not humidity-corrected.
inline constexpr float kSpeedOfSoundDryAir20C = 343.f;

/// IEC reference sound pressure for SPL in air (20 μPa).
inline constexpr float kReferenceSoundPressurePa = 20e-6f;

/// Wavelength λ = v / f (meters). Returns 0 if `frequencyHz` or `speedOfSoundMps` is not positive.
[[nodiscard]] inline float wavelengthMeters(float frequencyHz,
                                            float speedOfSoundMps = kSpeedOfSoundDryAir20C) noexcept {
    if (frequencyHz <= 0.f || speedOfSoundMps <= 0.f) {
        return 0.f;
    }
    return speedOfSoundMps / frequencyHz;
}

/// f = v / λ (Hz). Returns 0 if `wavelengthMeters` or `speedOfSoundMps` is not positive.
[[nodiscard]] inline float frequencyFromWavelengthHz(float wavelengthMeters,
                                                       float speedOfSoundMps = kSpeedOfSoundDryAir20C) noexcept {
    if (wavelengthMeters <= 0.f || speedOfSoundMps <= 0.f) {
        return 0.f;
    }
    return speedOfSoundMps / wavelengthMeters;
}

/// Period T = 1 / f (seconds). Returns 0 if `frequencyHz` is not positive.
[[nodiscard]] inline float periodSeconds(float frequencyHz) noexcept {
    if (frequencyHz <= 0.f) {
        return 0.f;
    }
    return 1.f / frequencyHz;
}

/// Amplitude ratio ↔ decibels using the **20 log10** rule (voltage / sound pressure style).
[[nodiscard]] inline float amplitudeRatioToDecibels(float linearRatio) noexcept {
    if (linearRatio <= 0.f) {
        return -std::numeric_limits<float>::infinity();
    }
    return 20.f * std::log10(linearRatio);
}

[[nodiscard]] inline float decibelsToAmplitudeRatio(float decibels) noexcept {
    return std::pow(10.f, decibels / 20.f);
}

/// SPL in dB re 20 μPa: 20 log10(p / p_ref). Non-positive `soundPressurePa` yields −∞.
[[nodiscard]] inline float soundPressureLevelDecibels(float soundPressurePa) noexcept {
    if (soundPressurePa <= 0.f) {
        return -std::numeric_limits<float>::infinity();
    }
    return 20.f * std::log10(soundPressurePa / kReferenceSoundPressurePa);
}

/// Inverse of [`soundPressureLevelDecibels`].
[[nodiscard]] inline float splToSoundPressurePascals(float splDecibels) noexcept {
    return kReferenceSoundPressurePa * decibelsToAmplitudeRatio(splDecibels);
}

/// Spherical spreading in a free field: acoustic pressure ~ 1/r, so scale = r_ref / r.
/// Returns 0 if either distance is not positive.
[[nodiscard]] inline float pointSourcePressureScale(float distanceMeters,
                                                    float referenceDistanceMeters = 1.f) noexcept {
    if (distanceMeters <= 0.f || referenceDistanceMeters <= 0.f) {
        return 0.f;
    }
    return referenceDistanceMeters / distanceMeters;
}

} // namespace marble::audio
