#pragma once

#include "input/Hid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace marble::input {

enum class AbstractControl : std::uint8_t {
    Start,
    BackSelect,
    LPadDown,
    LPadUp,
    LPadLeft,
    LPadRight,
    RPadDown,
    RPadUp,
    RPadLeft,
    RPadRight,
    LStickButton,
    RStickButton,
    LShoulder,
    RShoulder,
    LStickX,
    LStickY,
    RStickX,
    RStickY,
    LTrigger,
    RTrigger,
    Count
};

enum class ControlValueClass : std::uint8_t {
    DigitalButton,
    UniAxis,
    BiAxis,
    RelativeAxis
};

[[nodiscard]] constexpr std::size_t toIndex(AbstractControl c) noexcept {
    return static_cast<std::size_t>(c);
}

[[nodiscard]] constexpr ControlValueClass valueClassOf(AbstractControl c) noexcept {
    switch (c) {
        case AbstractControl::LStickX:
        case AbstractControl::LStickY:
        case AbstractControl::RStickX:
        case AbstractControl::RStickY:
            return ControlValueClass::BiAxis;
        case AbstractControl::LTrigger:
        case AbstractControl::RTrigger:
            return ControlValueClass::UniAxis;
        default:
            return ControlValueClass::DigitalButton;
    }
}

using LogicalActionId = std::uint16_t;
static constexpr LogicalActionId kInvalidAction = static_cast<LogicalActionId>(0xFFFFu);

struct ActionBinding {
    LogicalActionId action{kInvalidAction};
    ControlValueClass valueClass{ControlValueClass::DigitalButton};
    bool invert{false};
};

enum class LogicalDevice : std::uint8_t {
    Player = 0,
    Camera = 1,
    Menu = 2,
    Count
};

[[nodiscard]] constexpr std::uint32_t deviceMask(LogicalDevice d) noexcept {
    return (1u << static_cast<std::uint32_t>(d));
}

/// Maps abstract controls to logical game actions with class compatibility checks.
class InputRemapTable {
public:
    [[nodiscard]] bool bind(AbstractControl control, ActionBinding binding) noexcept {
        if (binding.action == kInvalidAction) {
            return false;
        }
        if (binding.valueClass != valueClassOf(control)) {
            return false;
        }
        bindings_[toIndex(control)] = binding;
        return true;
    }

    void unbind(AbstractControl control) noexcept {
        bindings_[toIndex(control)] = std::nullopt;
    }

    [[nodiscard]] std::optional<ActionBinding> bindingOf(AbstractControl control) const noexcept {
        return bindings_[toIndex(control)];
    }

private:
    std::array<std::optional<ActionBinding>, toIndex(AbstractControl::Count)> bindings_{};
};

/// One row applied by `ActionPolicy::applyActionContext` (ownership + disabled together).
struct ActionContextEntry {
    LogicalActionId action{kInvalidAction};
    std::uint32_t ownerMask{0u};
    bool disabled{false};
};

/// Context/ownership and action-disable policy applied at logical-action layer.
template <std::size_t MaxActions = 256>
class ActionPolicy {
public:
    static_assert(MaxActions > 0, "MaxActions must be > 0");

    /// Clear owner masks and disabled flags for all actions (shared access, all enabled).
    void resetActionGates() noexcept {
        ownerMasks_.fill(0u);
        for (auto& v : disabled_) {
            v = false;
        }
    }

    /// Apply context rows; each listed action gets the given owner mask and disabled state.
    void applyActionContext(std::span<ActionContextEntry const> entries) noexcept {
        for (ActionContextEntry const& e : entries) {
            if (e.action >= MaxActions) {
                continue;
            }
            ownerMasks_[e.action] = e.ownerMask;
            disabled_[e.action] = e.disabled;
        }
    }

    void setOwnerMask(LogicalActionId action, std::uint32_t ownerMask) noexcept {
        if (action >= MaxActions) {
            return;
        }
        ownerMasks_[action] = ownerMask;
    }

    [[nodiscard]] std::uint32_t ownerMaskOf(LogicalActionId action) const noexcept {
        if (action >= MaxActions) {
            return 0u;
        }
        return ownerMasks_[action];
    }

    void setDisabled(LogicalActionId action, bool disabled) noexcept {
        if (action >= MaxActions) {
            return;
        }
        disabled_[action] = disabled;
    }

    /// Fail-safe to avoid "controls locked forever" scenarios.
    void clearAllDisabled() noexcept {
        for (auto& v : disabled_) {
            v = false;
        }
    }

    [[nodiscard]] bool isAllowed(LogicalActionId action, LogicalDevice requester) const noexcept {
        if (action >= MaxActions) {
            return false;
        }
        if (disabled_[action]) {
            return false;
        }
        const std::uint32_t owners = ownerMasks_[action];
        if (owners == 0u) {
            return true; // no explicit ownership means shared access.
        }
        return (owners & deviceMask(requester)) != 0u;
    }

private:
    std::array<std::uint32_t, MaxActions> ownerMasks_{};
    std::array<bool, MaxActions> disabled_{};
};

/// Minimal controller-to-player router for multi-HID management.
template <std::size_t MaxControllers = 4, std::size_t MaxPlayers = 4>
class ControllerRouter {
public:
    static_assert(MaxControllers > 0, "MaxControllers must be > 0");
    static_assert(MaxPlayers > 0, "MaxPlayers must be > 0");

    void setAttached(std::size_t controllerIndex, bool attached) noexcept {
        if (controllerIndex >= MaxControllers) {
            return;
        }
        attached_[controllerIndex] = attached;
        if (!attached) {
            controllerToPlayer_[controllerIndex] = kUnassigned;
        }
    }

    [[nodiscard]] bool assign(std::size_t controllerIndex, std::size_t playerIndex) noexcept {
        if (controllerIndex >= MaxControllers || playerIndex >= MaxPlayers) {
            return false;
        }
        if (!attached_[controllerIndex]) {
            return false;
        }

        for (std::size_t c = 0; c < MaxControllers; ++c) {
            if (controllerToPlayer_[c] == static_cast<int>(playerIndex)) {
                controllerToPlayer_[c] = kUnassigned;
            }
        }

        controllerToPlayer_[controllerIndex] = static_cast<int>(playerIndex);
        return true;
    }

    void assignOneToOne() noexcept {
        for (std::size_t i = 0; i < MaxControllers && i < MaxPlayers; ++i) {
            if (attached_[i]) {
                controllerToPlayer_[i] = static_cast<int>(i);
            } else {
                controllerToPlayer_[i] = kUnassigned;
            }
        }
    }

    [[nodiscard]] int playerForController(std::size_t controllerIndex) const noexcept {
        if (controllerIndex >= MaxControllers) {
            return kUnassigned;
        }
        return controllerToPlayer_[controllerIndex];
    }

    [[nodiscard]] int controllerForPlayer(std::size_t playerIndex) const noexcept {
        if (playerIndex >= MaxPlayers) {
            return kUnassigned;
        }
        for (std::size_t c = 0; c < MaxControllers; ++c) {
            if (controllerToPlayer_[c] == static_cast<int>(playerIndex)) {
                return static_cast<int>(c);
            }
        }
        return kUnassigned;
    }

private:
    static constexpr int kUnassigned = -1;
    std::array<bool, MaxControllers> attached_{};
    std::array<int, MaxControllers> controllerToPlayer_{
        [] {
            std::array<int, MaxControllers> v{};
            for (auto& x : v) x = kUnassigned;
            return v;
        }()
    };
};

} // namespace marble::input
