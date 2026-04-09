# ADR-0030: HID context ownership and input-disable policy (Chapter 9 §9.5.8–§9.5.9)

## Status

Accepted

## Context

With abstraction/remapping in place (ADR-0029), the next HID concerns are:

- context-sensitive ownership of controls (player/camera/menu),
- temporary input suppression without globally masking physical device reads.

The chapter warns that heavy-handed device-level disable masks can strand players without control if not cleared.

## Decision

1. Extend `[input/HidMapping.hpp](../../engine/input/HidMapping.hpp)` with:
  - `LogicalDevice` roles (`Player`, `Camera`, `Menu`),
  - `ActionPolicy` for per-action owner masks and enable/disable state.
2. Enforce disable/ownership checks at **logical action** level, not raw HID input level.
3. Provide `clearAllDisabled()` fail-safe to recover from stale disable states (e.g., after respawn/reset).

## Consequences

- Positive: context ownership is explicit and testable.
- Positive: disabling one action no longer hides raw input from unrelated systems.
- Trade-off: this is policy infrastructure only; runtime state-machine integration remains separate.
- Engineering follow-up (implemented after initial acceptance): batched transitions via `ActionContextEntry`, `ActionPolicy::resetActionGates()`, and `applyActionContext(std::span<…>)` (`hid_action_context_test`); `marbles` applies separate gameplay vs victory context rows while continuing to sample keyboard + merged gamepad at the abstract-control layer (see [`MASTER_PLAN.md`](../MASTER_PLAN.md) §3 and [`notes/DECISION_LOG.md`](../notes/DECISION_LOG.md)).

## Alternatives considered

- **Physical-device disable masks:** rejected as too broad and failure-prone.
- **No ownership layer:** rejected because context-sensitive controls become ad-hoc and fragile.

