#include "audio/AudioSpatial3D.hpp"

#include <cmath>

namespace {

bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    using namespace marble::audio;
    using namespace marble::math;

    Listener3D L{};
    L.position = Vec3::zero();
    L.forward = Vec3::unitZ();
    L.worldUpHint = Vec3::unitY();

    const ListenerBasis b = buildListenerBasis(L);
    if (!b.valid) {
        return 1;
    }

    if (stereoPan11(b, Vec3{10.f, 0.f, 0.f}) != 1.f) {
        return 2;
    }
    if (stereoPan11(b, Vec3{-10.f, 0.f, 0.f}) != -1.f) {
        return 3;
    }
    if (!approx(stereoPan11(b, Vec3{0.f, 0.f, 10.f}), 0.f)) {
        return 4;
    }
    if (!approx(stereoPan11(b, Vec3{0.f, 5.f, 0.f}), 0.f)) {
        return 5;
    }

    Listener3D bad{};
    bad.forward = Vec3::zero();
    if (buildListenerBasis(bad).valid) {
        return 6;
    }

    if (!approx(distanceAttenuationLinear(1.f, 1.f, 10.f), 1.f)) {
        return 7;
    }
    if (!approx(distanceAttenuationLinear(10.f, 1.f, 10.f), 0.f)) {
        return 8;
    }
    if (!approx(distanceAttenuationLinear(5.5f, 1.f, 10.f), 0.5f)) {
        return 9;
    }

    if (!approx(distanceAttenuationInverseDistance(2.f, 1.f), 0.5f)) {
        return 10;
    }

    float gl = 0.f;
    float gr = 0.f;
    stereoSpeakerGainsLinear(0.f, 1.f, gl, gr);
    if (!approx(gl, 0.5f) || !approx(gr, 0.5f)) {
        return 11;
    }
    stereoSpeakerGainsLinear(1.f, 1.f, gl, gr);
    if (!approx(gl, 0.f) || !approx(gr, 1.f)) {
        return 12;
    }

    const float vr = radialVelocitySourceTowardListener(Vec3::zero(), Vec3{1.f, 0.f, 0.f}, Vec3{-10.f, 0.f, 0.f});
    if (!approx(vr, 10.f)) {
        return 13;
    }

    const float c = kSpeedOfSoundDryAir20C;
    if (!approx(dopplerPitchScale(c, 0.f), 1.f)) {
        return 14;
    }
    if (!approx(dopplerPitchScale(c, c * 0.1f), c / (c - c * 0.1f))) {
        return 15;
    }

    return 0;
}
