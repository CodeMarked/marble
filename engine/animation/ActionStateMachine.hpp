#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::animation {

/// One row of a Mealy-style action graph: if `current == fromState` and `trigger` fires, go to `toState`.
/// Rows are evaluated in order; **the first match wins** (priority = table order).
struct ActionTransition {
    std::uint16_t fromState{};
    std::uint16_t trigger{};
    std::uint16_t toState{};
};

/// Apply one trigger. Updates `currentState` and returns `true` if a transition fired.
[[nodiscard]] inline bool tryFireTransition(
    std::uint16_t& currentState,
    ActionTransition const* transitions,
    std::size_t transitionCount,
    std::uint16_t trigger
) noexcept {
    if (transitions == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < transitionCount; ++i) {
        ActionTransition const& t = transitions[i];
        if (t.fromState == currentState && t.trigger == trigger) {
            currentState = t.toState;
            return true;
        }
    }
    return false;
}

template <std::size_t N>
[[nodiscard]] inline bool tryFireTransition(
    std::uint16_t& currentState,
    std::array<ActionTransition, N> const& table,
    std::uint16_t trigger
) noexcept {
    return tryFireTransition(currentState, table.data(), N, trigger);
}

/// Lightweight view: holds current state index and a transition table reference.
class ActionStateMachine {
public:
    constexpr ActionStateMachine(
        std::uint16_t initialState,
        ActionTransition const* transitions,
        std::size_t transitionCount
    ) noexcept
        : current_(initialState)
        , transitions_(transitions)
        , transitionCount_(transitionCount) {}

    template <std::size_t N>
    constexpr explicit ActionStateMachine(std::uint16_t initialState, std::array<ActionTransition, N> const& table) noexcept
        : current_(initialState)
        , transitions_(table.data())
        , transitionCount_(N) {}

    [[nodiscard]] constexpr std::uint16_t currentState() const noexcept {
        return current_;
    }

    constexpr void setState(std::uint16_t s) noexcept {
        current_ = s;
    }

    [[nodiscard]] bool fire(std::uint16_t trigger) noexcept {
        return tryFireTransition(current_, transitions_, transitionCount_, trigger);
    }

private:
    std::uint16_t current_;
    ActionTransition const* transitions_;
    std::size_t transitionCount_;
};

} // namespace marble::animation
