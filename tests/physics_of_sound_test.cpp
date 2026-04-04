#include "audio/PhysicsOfSound.hpp"

#include <cmath>

namespace {

bool approx(float a, float b, float eps = 5e-4f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    using namespace marble::audio;

    // A4 wavelength in air @ 343 m/s: 343 / 440
    const float lam440 = wavelengthMeters(440.f);
    if (!approx(lam440, 343.f / 440.f)) {
        return 1;
    }
    if (!approx(frequencyFromWavelengthHz(lam440), 440.f)) {
        return 2;
    }
    if (!approx(periodSeconds(440.f), 1.f / 440.f)) {
        return 3;
    }

    if (wavelengthMeters(0.f) != 0.f || periodSeconds(-1.f) != 0.f) {
        return 4;
    }

    // 20 dB = ×10 amplitude
    if (!approx(amplitudeRatioToDecibels(10.f), 20.f)) {
        return 5;
    }
    if (!approx(decibelsToAmplitudeRatio(20.f), 10.f)) {
        return 6;
    }
    if (!approx(decibelsToAmplitudeRatio(0.f), 1.f)) {
        return 7;
    }

    // SPL round-trip (1 Pa is very loud but deterministic)
    const float p = 0.5f;
    const float spl = soundPressureLevelDecibels(p);
    if (!approx(splToSoundPressurePascals(spl), p, 1e-3f)) {
        return 8;
    }

    if (!std::isinf(amplitudeRatioToDecibels(0.f)) || amplitudeRatioToDecibels(0.f) >= 0.f) {
        return 9;
    }

    // 1/r: twice as far → half the relative pressure
    if (!approx(pointSourcePressureScale(2.f, 1.f), 0.5f)) {
        return 10;
    }
    if (pointSourcePressureScale(0.f, 1.f) != 0.f) {
        return 11;
    }

    return 0;
}
