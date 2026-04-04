#include "math/Random.hpp"

#include <cstdint>

int main() {
    // Deterministic sequence with same seed.
    marble::math::Rng a{1337u};
    marble::math::Rng b{1337u};
    for (int i = 0; i < 8; ++i) {
        if (a() != b()) {
            return 1;
        }
    }

    marble::math::Rng rng{7u};
    for (int i = 0; i < 32; ++i) {
        const float v = marble::math::uniform01(rng);
        if (v < 0.f || v >= 1.f) {
            return 2;
        }
    }

    for (int i = 0; i < 32; ++i) {
        const float v = marble::math::uniformRange(rng, -2.f, 3.f);
        if (v < -2.f || v > 3.f) {
            return 3;
        }
    }

    for (int i = 0; i < 32; ++i) {
        const std::uint32_t v = marble::math::uniformU32(rng, 10u, 20u);
        if (v < 10u || v > 20u) {
            return 4;
        }
    }

    marble::math::Lcg32 lcg1{5u};
    marble::math::Lcg32 lcg2{5u};
    for (int i = 0; i < 8; ++i) {
        if (lcg1.nextU32() != lcg2.nextU32()) {
            return 5;
        }
    }

    return 0;
}
