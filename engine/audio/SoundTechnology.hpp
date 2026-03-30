#pragma once

#include <cstddef>
#include <cstdint>

namespace marble::audio {

/// Typical channel counts for playback layouts (mono / stereo).
enum class SpeakerLayout : std::uint8_t {
    Mono = 1,
    Stereo = 2,
};

/// Bytes for one sample of one channel. Zero if `bitsPerSample` is not a positive multiple of 8.
[[nodiscard]] inline std::uint32_t pcmBytesPerSample(std::uint32_t bitsPerSample) noexcept {
    if (bitsPerSample == 0 || (bitsPerSample % 8u) != 0u) {
        return 0u;
    }
    return bitsPerSample / 8u;
}

/// Interleaved PCM: one frame is all channels at one time instant. Zero if invalid.
[[nodiscard]] inline std::uint32_t pcmBytesPerFrame(std::uint32_t channelCount, std::uint32_t bitsPerSample) noexcept {
    const std::uint32_t bps = pcmBytesPerSample(bitsPerSample);
    if (bps == 0u || channelCount == 0u) {
        return 0u;
    }
    return channelCount * bps;
}

/// Total byte size for `frameCount` interleaved frames.
[[nodiscard]] inline std::uint64_t pcmByteSizeForFrames(std::uint64_t frameCount, std::uint32_t bytesPerFrame) noexcept {
    return frameCount * static_cast<std::uint64_t>(bytesPerFrame);
}

/// Reasonable PCM stream parameters for engine-side validation (not an exhaustive driver capability matrix).
[[nodiscard]] inline bool pcmStreamParamsValid(std::uint32_t sampleRateHz,
                                               std::uint32_t channelCount,
                                               std::uint32_t bitsPerSample) noexcept {
    if (sampleRateHz == 0u || channelCount == 0u || channelCount > 16u) {
        return false;
    }
    if (pcmBytesPerSample(bitsPerSample) == 0u || bitsPerSample > 32u) {
        return false;
    }
    return true;
}

/// Write one stereo frame (L, R) into interleaved int16 PCM at `frameIndex`.
inline void interleavedStereoWriteInt16(std::int16_t* interleaved, std::size_t frameIndex, std::int16_t left,
                                        std::int16_t right) noexcept {
    interleaved[frameIndex * 2u] = left;
    interleaved[frameIndex * 2u + 1u] = right;
}

/// Read one stereo frame from interleaved int16 PCM at `frameIndex`.
inline void interleavedStereoReadInt16(std::int16_t const* interleaved, std::size_t frameIndex, std::int16_t& left,
                                       std::int16_t& right) noexcept {
    left = interleaved[frameIndex * 2u];
    right = interleaved[frameIndex * 2u + 1u];
}

/// Simple digital mix of two normalized float samples into [-1, 1] (headroom / limiter deferred).
[[nodiscard]] inline float mixAddClamped(float a, float b) noexcept {
    const float s = a + b;
    if (s > 1.f) {
        return 1.f;
    }
    if (s < -1.f) {
        return -1.f;
    }
    return s;
}

} // namespace marble::audio
