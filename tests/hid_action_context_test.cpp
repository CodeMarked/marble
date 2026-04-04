#include "input/HidMapping.hpp"

#include <span>

int main() {
    using marble::input::ActionContextEntry;
    using marble::input::ActionPolicy;
    using marble::input::LogicalDevice;
    using marble::input::deviceMask;

    constexpr marble::input::LogicalActionId kTilt = 7;

    marble::input::ActionPolicy<32> policy;

    static constexpr ActionContextEntry kPlay[] = {
        {kTilt, deviceMask(LogicalDevice::Player), false},
    };
    static constexpr ActionContextEntry kUi[] = {
        {kTilt, deviceMask(LogicalDevice::Menu), true},
    };

    policy.resetActionGates();
    policy.applyActionContext(std::span{kPlay});

    if (!policy.isAllowed(kTilt, LogicalDevice::Player)) {
        return 1;
    }
    if (policy.isAllowed(kTilt, LogicalDevice::Menu)) {
        return 2;
    }

    policy.resetActionGates();
    policy.applyActionContext(std::span{kUi});
    if (policy.isAllowed(kTilt, LogicalDevice::Menu)) {
        return 3;
    }

    policy.clearAllDisabled();
    if (!policy.isAllowed(kTilt, LogicalDevice::Menu)) {
        return 4;
    }

    return 0;
}
