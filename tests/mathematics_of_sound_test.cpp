#include "audio/MathematicsOfSound.hpp"

#include <cmath>
#include <cstdint>

namespace {

bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    using namespace marble::audio;

    if (!approx(nyquistFrequencyHz(48'000.f), 24'000.f)) {
        return 1;
    }
    if (nyquistFrequencyHz(0.f) != 0.f) {
        return 2;
    }

    if (!approx(angularFrequencyRadiansPerSecond(1.f), kTwoPi)) {
        return 3;
    }

    // One cycle of 440 Hz in 1/440 s → phase wraps near 0 (floats rarely hit 2π exactly).
    const float ph = advancePhaseRadians(0.f, 440.f, 1.f / 440.f);
    if (!approx(monoSineSample(wrapPhaseTwoPi(ph)), 0.f, 1e-3f)) {
        return 4;
    }

    if (!approx(monoSineSample(0.f), 0.f)) {
        return 5;
    }
    if (!approx(monoSineSample(kTwoPi * 0.25f), 1.f)) {
        return 6;
    }

    if (!approx(durationSecondsForSampleCount(48'000.f, 48'000.f), 1.f)) {
        return 7;
    }
    if (sampleCountCeilForDuration(1.f, 48'000.f) != 48'000u) {
        return 8;
    }
    if (sampleCountCeilForDuration(0.f, 48'000.f) != 0u) {
        return 9;
    }

    if (!approx(linearInterpolateSamples(0.f, 10.f, 0.25f), 2.5f)) {
        return 10;
    }

    if (floatSampleToInt16(0.f) != 0) {
        return 11;
    }
    if (floatSampleToInt16(1.f) != 32767) {
        return 12;
    }
    if (floatSampleToInt16(-1.f) != static_cast<std::int16_t>(-32768)) {
        return 13;
    }
    const float back = int16ToFloatSample(floatSampleToInt16(0.25f));
    if (!approx(back, 0.25f, 2e-5f)) {
        return 14;
    }

    if (!approx(wrapPhaseTwoPi(-kTwoPi * 0.25f), kTwoPi * 0.75f, 1e-4f)) {
        return 15;
    }

    return 0;
}
