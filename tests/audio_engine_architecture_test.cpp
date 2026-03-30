#include "audio/AudioEngineArchitecture.hpp"

#include <cmath>

namespace {

bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    using namespace marble::audio;
    using namespace marble::math;

    AudioMixerGraph<4> g{};
    if (g.capacity() != 4u || g.activeVoiceCount() != 0u) {
        return 1;
    }

    AudioVoice center{};
    center.active = true;
    center.spatialized = true;
    center.pan11 = 0.f;
    center.dryGain = 1.f;
    center.distanceGain = 1.f;
    if (!g.setVoice(0, center)) {
        return 2;
    }

    StereoBus b = g.mixStereoSample();
    if (!approx(b.left, 0.5f) || !approx(b.right, 0.5f)) {
        return 3;
    }

    AudioVoice right = center;
    right.pan11 = 1.f;
    if (!g.setVoice(1, right)) {
        return 4;
    }
    b = g.mixStereoSample();
    if (!approx(b.left, 0.5f) || !approx(b.right, 1.f)) {
        return 5;
    }

    if (g.setVoice(99u, center) || g.voice(99u) != nullptr || g.clearVoice(99u)) {
        return 6;
    }

    g.clearAll();
    if (g.activeVoiceCount() != 0u) {
        return 7;
    }

    Listener3D L{};
    L.position = Vec3::zero();
    L.forward = Vec3::unitZ();
    L.worldUpHint = Vec3::unitY();
    const ListenerBasis basis = buildListenerBasis(L);
    if (!basis.valid) {
        return 8;
    }

    const AudioVoice sv = makeSpatialVoice(basis, Vec3{10.f, 0.f, 0.f}, Vec3{-5.f, 0.f, 0.f}, 0.8f, 1.f, 30.f);
    if (!sv.active || !sv.spatialized) {
        return 9;
    }
    if (!(sv.pan11 > 0.9f && sv.pan11 <= 1.f)) {
        return 10;
    }
    if (!(sv.distanceGain > 0.f && sv.distanceGain < 1.f)) {
        return 11;
    }
    if (!(sv.pitchScale > 1.f)) {
        return 12;
    }

    // Non-spatialized voices ignore pan and feed center.
    AudioVoice ui{};
    ui.active = true;
    ui.spatialized = false;
    ui.pan11 = -1.f;
    ui.dryGain = 0.6f;
    ui.distanceGain = 1.f;
    g.clearAll();
    g.setVoice(0, ui);
    b = g.mixStereoSample();
    if (!approx(b.left, 0.3f) || !approx(b.right, 0.3f)) {
        return 13;
    }

    return 0;
}
