#include "animation/ActionStateMachine.hpp"

#include <array>
#include <cstdint>

int main() {
    using marble::animation::ActionStateMachine;
    using marble::animation::ActionTransition;
    using marble::animation::tryFireTransition;

    enum : std::uint16_t {
        StIdle = 0,
        StWalk = 1,
        StRun = 2,
    };
    enum : std::uint16_t {
        TrMove = 10,
        TrSprint = 11,
        TrStop = 12,
        TrNoop = 99,
    };

    const std::array<ActionTransition, 4> table{{
        {StIdle, TrMove, StWalk},
        {StWalk, TrSprint, StRun},
        {StWalk, TrStop, StIdle},
        {StRun, TrStop, StWalk},
    }};

    std::uint16_t s = StIdle;
    if (tryFireTransition(s, table, TrMove) != true || s != StWalk) {
        return 1;
    }
    if (tryFireTransition(s, table, TrSprint) != true || s != StRun) {
        return 2;
    }
    if (tryFireTransition(s, table, TrStop) != true || s != StWalk) {
        return 3;
    }
    if (tryFireTransition(s, table, TrStop) != true || s != StIdle) {
        return 4;
    }
    if (tryFireTransition(s, table, TrSprint) != false || s != StIdle) {
        return 5;
    }

    ActionStateMachine sm(StIdle, table);
    if (sm.currentState() != StIdle) {
        return 6;
    }
    if (!sm.fire(TrMove) || sm.currentState() != StWalk) {
        return 7;
    }
    sm.setState(StRun);
    if (sm.currentState() != StRun) {
        return 8;
    }

    std::uint16_t x = 0;
    if (tryFireTransition(x, static_cast<ActionTransition const*>(nullptr), 0, 1) != false) {
        return 9;
    }

    const std::array<ActionTransition, 2> priority{{
        {StIdle, TrMove, StWalk},
        {StIdle, TrMove, StRun},
    }};
    std::uint16_t p = StIdle;
    if (!tryFireTransition(p, priority, TrMove) || p != StWalk) {
        return 10;
    }

    return 0;
}
