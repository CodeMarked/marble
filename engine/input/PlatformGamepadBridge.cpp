#include "input/PlatformGamepadBridge.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace marble::input {

namespace {

constexpr float kStickDeadzone = 0.15f;
constexpr float kPadThreshold = 0.35f;

[[nodiscard]] float deadzoneAxis(float v) noexcept {
    float const a = std::fabs(v);
    if (a < kStickDeadzone) {
        return 0.f;
    }
    float const s = v >= 0.f ? 1.f : -1.f;
    return s * (a - kStickDeadzone) / (1.f - kStickDeadzone);
}

void mergeDigital(AbstractControlArray& io, AbstractControl c, float v) noexcept {
    std::size_t const i = static_cast<std::size_t>(c);
    io[i] = std::max(io[i], std::clamp(v, 0.f, 1.f));
}

void mergeSignedAxis(AbstractControlArray& io, AbstractControl c, float v) noexcept {
    std::size_t const i = static_cast<std::size_t>(c);
    if (std::fabs(v) > std::fabs(io[i])) {
        io[i] = std::clamp(v, -1.f, 1.f);
    }
}

void mergeStickAsDpad(AbstractControlArray& io, float x, float y) noexcept {
    if (y >= kPadThreshold) {
        mergeDigital(io, AbstractControl::LPadUp, 1.f);
    }
    if (y <= -kPadThreshold) {
        mergeDigital(io, AbstractControl::LPadDown, 1.f);
    }
    if (x <= -kPadThreshold) {
        mergeDigital(io, AbstractControl::LPadLeft, 1.f);
    }
    if (x >= kPadThreshold) {
        mergeDigital(io, AbstractControl::LPadRight, 1.f);
    }
}

void mergeGamepadStateInto(AbstractControlArray& io, GLFWgamepadstate const& st) noexcept {
    float const lx = deadzoneAxis(st.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    float const ly = deadzoneAxis(st.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    float const rx = deadzoneAxis(st.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
    float const ry = deadzoneAxis(st.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
    float const lt = std::clamp((st.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.f) * 0.5f, 0.f, 1.f);
    float const rt = std::clamp((st.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.f) * 0.5f, 0.f, 1.f);

    mergeSignedAxis(io, AbstractControl::LStickX, lx);
    mergeSignedAxis(io, AbstractControl::LStickY, ly);
    mergeSignedAxis(io, AbstractControl::RStickX, rx);
    mergeSignedAxis(io, AbstractControl::RStickY, ry);
    mergeDigital(io, AbstractControl::LTrigger, lt);
    mergeDigital(io, AbstractControl::RTrigger, rt);

    mergeStickAsDpad(io, lx, ly);

    auto const btn = [&](int b) -> float {
        return st.buttons[b] == GLFW_PRESS ? 1.f : 0.f;
    };
    mergeDigital(io, AbstractControl::LPadUp, btn(GLFW_GAMEPAD_BUTTON_DPAD_UP));
    mergeDigital(io, AbstractControl::LPadDown, btn(GLFW_GAMEPAD_BUTTON_DPAD_DOWN));
    mergeDigital(io, AbstractControl::LPadLeft, btn(GLFW_GAMEPAD_BUTTON_DPAD_LEFT));
    mergeDigital(io, AbstractControl::LPadRight, btn(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT));
    mergeDigital(io, AbstractControl::RPadDown, btn(GLFW_GAMEPAD_BUTTON_A));
    mergeDigital(io, AbstractControl::RPadRight, btn(GLFW_GAMEPAD_BUTTON_B));
    mergeDigital(io, AbstractControl::RPadLeft, btn(GLFW_GAMEPAD_BUTTON_X));
    mergeDigital(io, AbstractControl::RPadUp, btn(GLFW_GAMEPAD_BUTTON_Y));
    mergeDigital(io, AbstractControl::LShoulder, btn(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER));
    mergeDigital(io, AbstractControl::RShoulder, btn(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER));
    mergeDigital(io, AbstractControl::BackSelect, btn(GLFW_GAMEPAD_BUTTON_BACK));
    mergeDigital(io, AbstractControl::Start, btn(GLFW_GAMEPAD_BUTTON_START));
    mergeDigital(io, AbstractControl::LStickButton, btn(GLFW_GAMEPAD_BUTTON_LEFT_THUMB));
    mergeDigital(io, AbstractControl::RStickButton, btn(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB));
}

} // namespace

void mergeGamepadJoystickIntoAbstractControls(int glfwJoystickId, AbstractControlArray& io) noexcept {
    if (glfwJoystickId < GLFW_JOYSTICK_1 || glfwJoystickId > GLFW_JOYSTICK_LAST) {
        return;
    }
    if (glfwJoystickIsGamepad(glfwJoystickId) != GLFW_TRUE) {
        return;
    }
    GLFWgamepadstate st{};
    if (glfwGetGamepadState(glfwJoystickId, &st) != GLFW_TRUE) {
        return;
    }
    mergeGamepadStateInto(io, st);
}

void mergeAllConnectedGamepadsIntoAbstractControls(AbstractControlArray& io) noexcept {
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
        mergeGamepadJoystickIntoAbstractControls(j, io);
    }
}

bool anyStandardGamepadPresent() noexcept {
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
        if (glfwJoystickIsGamepad(j) == GLFW_TRUE) {
            return true;
        }
    }
    return false;
}

void mergeFirstGamepadIntoAbstractControls(AbstractControlArray& io) noexcept {
    mergeAllConnectedGamepadsIntoAbstractControls(io);
}

} // namespace marble::input
