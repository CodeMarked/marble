#pragma once

#include <cstdint>
#include <random>

namespace marble::math {

/// Fast but weaker quality generator (book §5.7.1). Keep for explicit legacy uses.
class Lcg32 {
public:
    explicit constexpr Lcg32(std::uint32_t seed) noexcept : state_(seed) {}

    [[nodiscard]] constexpr std::uint32_t nextU32() noexcept {
        // Numerical Recipes constants.
        state_ = state_ * 1664525u + 1013904223u;
        return state_;
    }

private:
    std::uint32_t state_{};
};

/// Default engine PRNG policy: Mersenne Twister quality/performance balance (book §5.7.2).
using Rng = std::mt19937;

[[nodiscard]] inline float uniform01(Rng& rng) noexcept {
    return std::generate_canonical<float, 24>(rng);
}

[[nodiscard]] inline float uniformRange(Rng& rng, float minValue, float maxValue) noexcept {
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(rng);
}

[[nodiscard]] inline std::uint32_t uniformU32(Rng& rng, std::uint32_t minValue, std::uint32_t maxValue) noexcept {
    std::uniform_int_distribution<std::uint32_t> dist(minValue, maxValue);
    return dist(rng);
}

} // namespace marble::math
