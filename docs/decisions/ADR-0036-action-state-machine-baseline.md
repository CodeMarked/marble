# ADR-0036: Action state machine baseline (Chapter 12 §12.10)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 036** in **§12.10 Action State Machines**. Gameplay and animation selection need a small, deterministic **finite transition layer** between named locomotion/action states without adopting a heavy visual tooling stack in this milestone.

## Decision

1. Add `[animation/ActionStateMachine.hpp](../../engine/animation/ActionStateMachine.hpp)`:
   - `ActionTransition` rows (`fromState`, `trigger`, `toState`) with **numeric** `uint16_t` ids (gameplay maps enums to these values),
   - `tryFireTransition` scanning a table in order (**first match wins** — explicit priority),
   - `ActionStateMachine` view class holding current state + table pointer for ergonomic `fire`.
2. Cover with `animation_action_state_machine_test`.

## Consequences

- Positive: transition logic is data-driven, trivial to unit test, and easy to serialize later.
- Positive: integrates with existing HID logical actions by treating resolved actions as `trigger` ids (separate wiring).
- Trade-off: no hierarchical state machines, transition guards/callbacks, or animation clip binding in this header.
- Follow-up: guard predicates, enter/exit hooks, blend requests per transition, and editor-facing graphs.

## Alternatives considered

- **Third-party state-machine library:** deferred to keep animation/control policy visible and dependency-free.
- **String state names in hot path:** rejected for the baseline; use hashed or numeric ids at runtime.
