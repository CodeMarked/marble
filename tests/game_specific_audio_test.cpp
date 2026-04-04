#include "audio/GameSpecificAudio.hpp"

#include <cmath>
#include <cstdint>

namespace {

bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    using namespace marble::audio;

    CategoryGains g{};
    g.music = 0.8f;
    g.sfx = 0.7f;
    g.ui = 1.0f;
    g.voice = 1.2f;

    if (!approx(gainForCategory(g, MixCategory::Music), 0.8f)) {
        return 1;
    }
    if (!approx(gainForCategory(g, MixCategory::Voice), 1.2f)) {
        return 2;
    }

    OneShotState state{};
    state.lastTriggerSeconds = 10.f;
    if (canTriggerOneShot(10.1f, state, 0.2f)) {
        return 3;
    }
    if (!canTriggerOneShot(10.3f, state, 0.2f)) {
        return 4;
    }

    state.variationCursor = 0u;
    if (nextVariationIndex(state, 3u) != 0u) {
        return 5;
    }
    if (nextVariationIndex(state, 3u) != 1u) {
        return 6;
    }
    if (nextVariationIndex(state, 3u) != 2u) {
        return 7;
    }
    if (nextVariationIndex(state, 3u) != 0u) {
        return 8;
    }

    OneShotSpec spec{};
    spec.category = MixCategory::Sfx;
    spec.baseGain = 0.5f;
    spec.cooldownSeconds = 0.1f;
    spec.variationCount = 2u;
    spec.spatialized = false;

    AudioVoice v{};
    std::uint32_t var = 99u;
    state = {};
    state.lastTriggerSeconds = 1.0f;
    if (!triggerOneShot(spec, 1.2f, state, gainForCategory(g, spec.category), v, var)) {
        return 9;
    }
    if (var != 0u) {
        return 10;
    }
    if (!v.active || v.spatialized) {
        return 11;
    }
    if (!approx(v.dryGain, 0.5f * 0.7f)) {
        return 12;
    }

    // Cooldown blocks immediate retrigger.
    if (triggerOneShot(spec, 1.25f, state, 1.f, v, var)) {
        return 13;
    }
    if (!triggerOneShot(spec, 1.35f, state, 1.f, v, var)) {
        return 14;
    }
    if (var != 1u) {
        return 15;
    }

    const CategoryGains ducked = applyVoiceOverDucking(g, true, 0.25f, 0.5f);
    if (!approx(ducked.music, 0.8f * 0.25f) || !approx(ducked.sfx, 0.7f * 0.5f)) {
        return 16;
    }
    const CategoryGains plain = applyVoiceOverDucking(g, false, 0.1f, 0.1f);
    if (!approx(plain.music, g.music) || !approx(plain.sfx, g.sfx)) {
        return 17;
    }

    return 0;
}
