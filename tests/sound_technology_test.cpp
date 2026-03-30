#include "audio/SoundTechnology.hpp"

#include <array>
#include <cstdint>

int main() {
    using namespace marble::audio;

    if (pcmBytesPerSample(16u) != 2u || pcmBytesPerSample(15u) != 0u) {
        return 1;
    }
    if (pcmBytesPerFrame(2u, 16u) != 4u || pcmBytesPerFrame(0u, 16u) != 0u) {
        return 2;
    }
    if (pcmByteSizeForFrames(1000u, 4u) != 4000u) {
        return 3;
    }

    if (!pcmStreamParamsValid(48'000u, 2u, 16u)) {
        return 4;
    }
    if (pcmStreamParamsValid(0u, 2u, 16u) || pcmStreamParamsValid(48'000u, 17u, 16u)) {
        return 5;
    }

    std::array<std::int16_t, 4> buf{};
    interleavedStereoWriteInt16(buf.data(), 0, 100, -200);
    interleavedStereoWriteInt16(buf.data(), 1, 300, 400);
    std::int16_t l = 0;
    std::int16_t r = 0;
    interleavedStereoReadInt16(buf.data(), 0, l, r);
    if (l != 100 || r != -200) {
        return 6;
    }
    interleavedStereoReadInt16(buf.data(), 1, l, r);
    if (l != 300 || r != 400) {
        return 7;
    }

    if (mixAddClamped(0.6f, 0.6f) != 1.f) {
        return 8;
    }
    if (mixAddClamped(-0.8f, -0.8f) != -1.f) {
        return 9;
    }
    if (mixAddClamped(0.25f, -0.5f) != -0.25f) {
        return 10;
    }

    return 0;
}
