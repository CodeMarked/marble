#pragma once

#include "audio/AudioEngineArchitecture.hpp"

#include <cstdint>

namespace marble::audio {

enum class MixCategory : std::uint8_t {
    Music = 0,
    Sfx = 1,
    Ui = 2,
    Voice = 3,
    Count
};

struct CategoryGains {
    float music{1.f};
    float sfx{1.f};
    float ui{1.f};
    float voice{1.f};
};

[[nodiscard]] inline float gainForCategory(CategoryGains const& g, MixCategory c) noexcept {
    switch (c) {
    case MixCategory::Music:
        return g.music;
    case MixCategory::Sfx:
        return g.sfx;
    case MixCategory::Ui:
        return g.ui;
    case MixCategory::Voice:
        return g.voice;
    default:
        return 1.f;
    }
}

/// One-shot trigger state to avoid retrigger spam and apply deterministic variation.
struct OneShotState {
    float lastTriggerSeconds{-1e30f};
    std::uint32_t variationCursor{};
};

/// Returns true if enough time elapsed since the previous trigger.
[[nodiscard]] inline bool canTriggerOneShot(float nowSeconds, OneShotState const& state, float cooldownSeconds) noexcept {
    if (cooldownSeconds <= 0.f) {
        return true;
    }
    return (nowSeconds - state.lastTriggerSeconds) >= cooldownSeconds;
}

/// Returns [0, variationCount) and advances a deterministic cursor.
[[nodiscard]] inline std::uint32_t nextVariationIndex(OneShotState& state, std::uint32_t variationCount) noexcept {
    if (variationCount == 0u) {
        return 0u;
    }
    const std::uint32_t idx = state.variationCursor % variationCount;
    state.variationCursor = (state.variationCursor + 1u) % variationCount;
    return idx;
}

/// Simple game-facing one-shot command mapped to mixer voice parameters.
struct OneShotSpec {
    MixCategory category{MixCategory::Sfx};
    float baseGain{1.f};
    float cooldownSeconds{};
    std::uint32_t variationCount{1u};
    bool spatialized{true};
};

/// Encodes event policy into a voice; returns false when gated by cooldown.
inline bool triggerOneShot(OneShotSpec const& spec,
                           float nowSeconds,
                           OneShotState& state,
                           float categoryGain,
                           AudioVoice& outVoice,
                           std::uint32_t& outVariationIndex) noexcept {
    if (!canTriggerOneShot(nowSeconds, state, spec.cooldownSeconds)) {
        return false;
    }

    outVariationIndex = nextVariationIndex(state, spec.variationCount);
    state.lastTriggerSeconds = nowSeconds;

    outVoice = {};
    outVoice.active = true;
    outVoice.spatialized = spec.spatialized;
    outVoice.dryGain = spec.baseGain * categoryGain;
    outVoice.distanceGain = 1.f;
    outVoice.pitchScale = 1.f;
    return true;
}

/// Game-side ducking policy: voice-over can attenuate music and SFX.
[[nodiscard]] inline CategoryGains applyVoiceOverDucking(CategoryGains base,
                                                         bool voiceOverActive,
                                                         float musicDuckGain,
                                                         float sfxDuckGain) noexcept {
    if (!voiceOverActive) {
        return base;
    }
    base.music *= musicDuckGain;
    base.sfx *= sfxDuckGain;
    return base;
}

} // namespace marble::audio
