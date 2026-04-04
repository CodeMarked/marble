#include "input/HidMapping.hpp"

int main() {
    constexpr marble::input::LogicalActionId kMove = 10;
    constexpr marble::input::LogicalActionId kLook = 11;
    constexpr marble::input::LogicalActionId kPause = 12;

    marble::input::ActionPolicy<64> policy;

    policy.setOwnerMask(
        kMove,
        marble::input::deviceMask(marble::input::LogicalDevice::Player));
    policy.setOwnerMask(
        kLook,
        marble::input::deviceMask(marble::input::LogicalDevice::Camera));
    policy.setOwnerMask(
        kPause,
        marble::input::deviceMask(marble::input::LogicalDevice::Menu));

    if (!policy.isAllowed(kMove, marble::input::LogicalDevice::Player)) {
        return 1;
    }
    if (policy.isAllowed(kMove, marble::input::LogicalDevice::Camera)) {
        return 2;
    }
    if (!policy.isAllowed(kLook, marble::input::LogicalDevice::Camera)) {
        return 3;
    }
    if (policy.isAllowed(kPause, marble::input::LogicalDevice::Player)) {
        return 4;
    }

    // Disabling is per logical action, regardless of owner.
    policy.setDisabled(kMove, true);
    if (policy.isAllowed(kMove, marble::input::LogicalDevice::Player)) {
        return 5;
    }

    // Fail-safe clear must restore disabled actions.
    policy.clearAllDisabled();
    if (!policy.isAllowed(kMove, marble::input::LogicalDevice::Player)) {
        return 6;
    }

    return 0;
}
