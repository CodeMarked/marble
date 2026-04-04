#pragma once

#include "audio/AudioSpatial3D.hpp"
#include "audio/SoundTechnology.hpp"

#include <array>
#include <cstddef>

namespace marble::audio {

/// Per-source runtime params evaluated by the engine mix graph.
struct AudioVoice {
    bool active{};
    bool spatialized{true};
    float dryGain{1.f};
    float distanceGain{1.f};
    float pan11{};
    float pitchScale{1.f};
};

/// Primary mix bus output in normalized float stereo.
struct StereoBus {
    float left{};
    float right{};
};

/// Fixed-capacity voice mixer baseline (chapter 14.5 architecture seam):
/// gameplay updates voices, audio backend consumes rendered bus frames.
template <std::size_t MaxVoices>
class AudioMixerGraph {
public:
    static_assert(MaxVoices > 0, "AudioMixerGraph requires at least one voice");

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return MaxVoices;
    }

    [[nodiscard]] std::size_t activeVoiceCount() const noexcept {
        std::size_t n = 0;
        for (AudioVoice const& v : voices_) {
            if (v.active) {
                ++n;
            }
        }
        return n;
    }

    bool setVoice(std::size_t index, AudioVoice voice) noexcept {
        if (index >= MaxVoices) {
            return false;
        }
        voices_[index] = voice;
        return true;
    }

    [[nodiscard]] AudioVoice const* voice(std::size_t index) const noexcept {
        if (index >= MaxVoices) {
            return nullptr;
        }
        return &voices_[index];
    }

    bool clearVoice(std::size_t index) noexcept {
        if (index >= MaxVoices) {
            return false;
        }
        voices_[index] = {};
        return true;
    }

    void clearAll() noexcept {
        for (AudioVoice& v : voices_) {
            v = {};
        }
    }

    /// Mix active voices into one stereo bus sample using linear pan and clamped add.
    [[nodiscard]] StereoBus mixStereoSample() const noexcept {
        StereoBus bus{};
        for (AudioVoice const& v : voices_) {
            if (!v.active) {
                continue;
            }
            const float gain = v.dryGain * v.distanceGain;
            float l = 0.f;
            float r = 0.f;
            const float pan = v.spatialized ? v.pan11 : 0.f;
            stereoSpeakerGainsLinear(pan, gain, l, r);
            bus.left = mixAddClamped(bus.left, l);
            bus.right = mixAddClamped(bus.right, r);
        }
        return bus;
    }

private:
    std::array<AudioVoice, MaxVoices> voices_{};
};

/// Convenience helper: derive voice spatial params from listener/source state.
[[nodiscard]] inline AudioVoice makeSpatialVoice(ListenerBasis const& listener,
                                                 math::Vec3 sourcePosition,
                                                 math::Vec3 sourceVelocity,
                                                 float baseGain,
                                                 float minDistance,
                                                 float maxDistance) noexcept {
    AudioVoice v{};
    v.active = true;
    v.spatialized = true;

    const math::Vec3 rel = sourcePosition - listener.position;
    const float dist = math::length(rel);
    v.distanceGain = distanceAttenuationLinear(dist, minDistance, maxDistance);
    v.pan11 = stereoPan11(listener, sourcePosition);
    v.dryGain = baseGain;

    const float vr = radialVelocitySourceTowardListener(listener.position, sourcePosition, sourceVelocity);
    v.pitchScale = dopplerPitchScale(kSpeedOfSoundDryAir20C, vr);
    return v;
}

} // namespace marble::audio
