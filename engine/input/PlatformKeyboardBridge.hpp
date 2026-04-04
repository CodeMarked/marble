#pragma once

#include "input/HidMapping.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace marble::platform {
class Window;
}

namespace marble::input {

inline constexpr std::size_t kAbstractControlCount = static_cast<std::size_t>(AbstractControl::Count);

using AbstractControlArray = std::array<float, kAbstractControlCount>;

/// After `Window::pollEvents()`, map keyboard keys to abstract digital controls (0/1).
/// Axes (`LStick*`, triggers, etc.) are left at 0 until a stick/trigger source is wired.
void sampleKeyboardIntoAbstractControls(platform::Window const& window, AbstractControlArray& out) noexcept;

/// Aggregate scalar action strength in [0, 1] from current abstract control magnitudes.
template <std::size_t MaxActions>
[[nodiscard]] float actionScalar(
    LogicalActionId action,
    AbstractControlArray const& controls,
    InputRemapTable const& remap,
    ActionPolicy<MaxActions> const& policy,
    LogicalDevice requester
) noexcept {
    if (action == kInvalidAction || !policy.isAllowed(action, requester)) {
        return 0.f;
    }
    float best = 0.f;
    for (std::size_t i = 0; i < kAbstractControlCount; ++i) {
        auto const ctrl = static_cast<AbstractControl>(i);
        std::optional<ActionBinding> const b = remap.bindingOf(ctrl);
        if (!b.has_value() || b->action != action) {
            continue;
        }
        float v = controls[i];
        if (b->invert) {
            v = -v;
        }
        switch (b->valueClass) {
        case ControlValueClass::DigitalButton:
            best = std::max(best, std::clamp(v, 0.f, 1.f));
            break;
        case ControlValueClass::UniAxis:
        case ControlValueClass::BiAxis:
        case ControlValueClass::RelativeAxis:
            best = std::max(best, std::clamp(std::fabs(v), 0.f, 1.f));
            break;
        }
    }
    return std::clamp(best, 0.f, 1.f);
}

} // namespace marble::input
