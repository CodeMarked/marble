# ADR-0053: Scripting bindings seam and high-level game flow (Chapter 16 §16.9–§16.10)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 053** in **§16.9 Scripting** through **§16.10 High-Level Game Flow**. Marble has gameplay update and world seams but no explicit native/script boundary or session-level phase control.

## Decision

1. Add [`gameplay/ScriptingAndGameFlow.hpp`](../../engine/gameplay/ScriptingAndGameFlow.hpp):
   - `ScriptHost` — fixed-capacity table of `StringId` → native `ScriptProc` + `void*` user data; `registerBinding` / `invoke` with a scalar `ScriptArg` (integer seam).
   - `GameFlowPhase` and `gameFlowTransitionAllowed` — explicit allowed edges (boot → loading → playing ↔ paused → exiting).
   - `GameFlowController` — holds current phase; `tryTransition` enforces the table; `reset` returns to boot.
2. No bytecode VM, sandbox, or language runtime in this chunk; bindings are the integration surface future tooling can target.
3. Verify with `scripting_game_flow_test`.

## Consequences

- Positive: testable seams for script-driven callbacks and session flow without pulling a third-party language.
- Positive: flow transitions are centralized and easy to extend when menus/loaders exist.
- Trade-off: `ScriptArg` is a single integer; rich payloads need a separate channel (events, handles, or buffers) later.
- Follow-up: real script VM or data-driven command lists; richer payload bridges over events; tie flow transitions to platform/input and asset readiness.

## Alternatives considered

- **`std::function` bindings:** rejected for this baseline to avoid heavy headers and non-trivial callable storage in fixed slots.
- **Implicit permissive FSM:** rejected; explicit allow-list matches “high-level game flow” documentation and catches invalid jumps early.
