#pragma once

#include "input/PlatformKeyboardBridge.hpp"

#include <GLFW/glfw3.h>

#include <cstddef>

namespace marble::input {

/// Merge one GLFW joystick id when it is a standard-layout gamepad (`glfwJoystickIsGamepad`).
/// No-op if `glfwJoystickId` is out of range, not a gamepad, or `glfwGetGamepadState` fails.
void mergeGamepadJoystickIntoAbstractControls(int glfwJoystickId, AbstractControlArray& io) noexcept;

/// Merge every connected standard-layout gamepad into `io` (same merge rules as a single pad).
void mergeAllConnectedGamepadsIntoAbstractControls(AbstractControlArray& io) noexcept;

/// True if any `GLFW_JOYSTICK_*` reports `glfwJoystickIsGamepad`. Use to hide gamepad-only UI on keyboard setups.
[[nodiscard]] bool anyStandardGamepadPresent() noexcept;

/// Same as `mergeAllConnectedGamepadsIntoAbstractControls` (kept for call sites that used the old name).
void mergeFirstGamepadIntoAbstractControls(AbstractControlArray& io) noexcept;

/// Set each controller slot's attachment from GLFW (`glfwJoystickIsGamepad` for `GLFW_JOYSTICK_1 + slot`).
/// Slots beyond `GLFW_JOYSTICK_LAST` are marked not attached.
template <std::size_t MaxControllers, std::size_t MaxPlayers>
void syncControllerRouterGamepadSlots(ControllerRouter<MaxControllers, MaxPlayers>& router) noexcept {
    int const maxSlotOffset = GLFW_JOYSTICK_LAST - GLFW_JOYSTICK_1;
    for (std::size_t i = 0; i < MaxControllers; ++i) {
        if (static_cast<int>(i) > maxSlotOffset) {
            router.setAttached(i, false);
            continue;
        }
        int const jid = GLFW_JOYSTICK_1 + static_cast<int>(i);
        router.setAttached(i, glfwJoystickIsGamepad(jid) == GLFW_TRUE);
    }
}

} // namespace marble::input
