#include "input/PlatformGamepadBridge.hpp"

#include <GLFW/glfw3.h>

#include <cmath>

namespace {

[[nodiscard]] bool allFinite(marble::input::AbstractControlArray const& controls) noexcept {
    for (float v : controls) {
        if (std::isnan(v) || std::isinf(v)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    if (glfwInit() != GLFW_TRUE) {
        return 1;
    }

    marble::input::AbstractControlArray controls{};
    marble::input::ControllerRouter<4, 4> router{};
    marble::input::syncControllerRouterGamepadSlots(router);

    marble::input::mergeGamepadJoystickIntoAbstractControls(-1, controls);
    marble::input::mergeGamepadJoystickIntoAbstractControls(GLFW_JOYSTICK_LAST + 1, controls);
    if (!allFinite(controls)) {
        glfwTerminate();
        return 2;
    }

    marble::input::mergeAllConnectedGamepadsIntoAbstractControls(controls);
    if (!allFinite(controls)) {
        glfwTerminate();
        return 3;
    }

    marble::input::mergeFirstGamepadIntoAbstractControls(controls);
    if (!allFinite(controls)) {
        glfwTerminate();
        return 4;
    }

    glfwTerminate();
    return 0;
}
