#include "animation/AnimationCompression.hpp"

#include <cmath>
#include <cstdint>

namespace {

bool approx(float a, float b, float eps = 2e-4f) {
    return a - b <= eps && b - a <= eps;
}

} // namespace

int main() {
    using marble::animation::dequantizeTranslation;
    using marble::animation::dequantizeTranslationVec3;
    using marble::animation::dequantizeUnitInterval;
    using marble::animation::quantizeTranslation;
    using marble::animation::quantizeTranslationVec3;
    using marble::animation::quantizeUnitInterval;

    constexpr float kScale = 4096.f;

    const std::int16_t q = quantizeTranslation(1.25f, kScale);
    const float back = dequantizeTranslation(q, kScale);
    if (!approx(back, 1.25f, 3e-4f)) {
        return 1;
    }

    std::int16_t xyz[3]{};
    quantizeTranslationVec3(marble::math::Vec3{-0.5f, 2.f, 0.f}, kScale, xyz);
    const marble::math::Vec3 v = dequantizeTranslationVec3(xyz[0], xyz[1], xyz[2], kScale);
    if (!approx(v.x, -0.5f) || !approx(v.y, 2.f) || !approx(v.z, 0.f)) {
        return 2;
    }

    const std::int16_t huge = quantizeTranslation(1.0e9f, kScale);
    if (huge != 32767) {
        return 3;
    }

    if (dequantizeTranslation(0, 0.f) != 0.f) {
        return 4;
    }

    const std::uint16_t tq = quantizeUnitInterval(0.5f);
    const float tu = dequantizeUnitInterval(tq);
    if (!approx(tu, 0.5f, 2e-5f)) {
        return 5;
    }

    if (quantizeUnitInterval(-1.f) != 0u || quantizeUnitInterval(2.f) != 65535u) {
        return 6;
    }

    return 0;
}
