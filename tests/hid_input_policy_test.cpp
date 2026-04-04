#include "input/Hid.hpp"

#include <cmath>

namespace {

constexpr marble::input::ButtonBits kA = 0x0001u;
constexpr marble::input::ButtonBits kB = 0x0002u;

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
    marble::input::ButtonStateTracker buttons;
    buttons.update(0u);
    if (buttons.events().downs != 0u || buttons.events().ups != 0u) {
        return 1;
    }

    buttons.update(kA);
    if (!buttons.wentDown(kA) || buttons.wentUp(kA)) {
        return 2;
    }

    buttons.update(kA | kB);
    if (!buttons.chordDown(kA | kB)) {
        return 3;
    }

    buttons.update(kB);
    if (!buttons.wentUp(kA) || buttons.wentDown(kA)) {
        return 4;
    }

    if (!approx(marble::input::applyCenteredDeadZone(0.03f, 0.05f), 0.0f)) {
        return 5;
    }
    if (!approx(marble::input::applyCenteredDeadZone(0.2f, 0.05f), 0.2f)) {
        return 6;
    }
    if (!approx(marble::input::applyPositiveDeadZone(0.02f, 0.05f), 0.0f)) {
        return 7;
    }
    if (!approx(marble::input::applyPositiveDeadZone(0.7f, 0.05f), 0.7f)) {
        return 8;
    }

    const float f = marble::input::lowPassFilter(1.0f, 0.0f, 0.1f, 0.016f);
    if (f <= 0.0f || f >= 1.0f) {
        return 9;
    }

    marble::input::MovingAverage<float, 3> avg;
    avg.addSample(1.0f);
    avg.addSample(2.0f);
    avg.addSample(3.0f);
    if (!approx(avg.currentAverage(), 2.0f)) {
        return 10;
    }
    avg.addSample(6.0f); // last three are 2,3,6
    if (!approx(avg.currentAverage(), 11.0f / 3.0f, 1e-4f)) {
        return 11;
    }

    return 0;
}
