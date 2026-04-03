#include "input/PlatformKeyboardBridge.hpp"

int main() {
    using marble::input::AbstractControl;
    using marble::input::AbstractControlArray;
    using marble::input::ActionPolicy;
    using marble::input::ControlValueClass;
    using marble::input::InputRemapTable;
    using marble::input::LogicalDevice;
    using marble::input::actionScalar;

    marble::input::InputRemapTable table;
    if (!table.bind(
            AbstractControl::LPadUp,
            {1u, ControlValueClass::DigitalButton, false})) {
        return 1;
    }
    if (!table.bind(
            AbstractControl::LPadDown,
            {2u, ControlValueClass::DigitalButton, false})) {
        return 2;
    }

    marble::input::ActionPolicy<256> policy;
    AbstractControlArray controls{};
    controls[static_cast<std::size_t>(AbstractControl::LPadUp)] = 1.f;

    float const up = actionScalar(1u, controls, table, policy, LogicalDevice::Player);
    if (up < 0.99f || up > 1.01f) {
        return 3;
    }
    float const missing = actionScalar(2u, controls, table, policy, LogicalDevice::Player);
    if (missing != 0.f) {
        return 4;
    }

    policy.setDisabled(1u, true);
    float const disabled = actionScalar(1u, controls, table, policy, LogicalDevice::Player);
    if (disabled != 0.f) {
        return 5;
    }

    return 0;
}
