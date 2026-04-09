# ADR-0029: HID multi-device abstraction and remapping baseline (Chapter 9 §9.5.5–§9.5.7)

## Status

Accepted

## Context

After establishing input processing and gesture detection (ADR-0027/0028), the next HID concerns are:
- managing multiple controllers and player assignment,
- insulating gameplay from platform-specific control layouts,
- remapping abstract controls to logical game actions with type compatibility.

## Decision

1. Add [`input/HidMapping.hpp`](../../engine/input/HidMapping.hpp) with:
   - `AbstractControl` enum for platform-agnostic controls,
   - `ControlValueClass` classification (`DigitalButton`, `UniAxis`, `BiAxis`, `RelativeAxis`),
   - `InputRemapTable` enforcing value-class compatibility for action bindings,
   - `ControllerRouter` for controller attach/assign/unassign and one-to-one bootstrap mapping.
2. Keep this layer data-oriented and platform-neutral (no GLFW/XInput APIs in this baseline).
3. Treat one-controller-per-player as the default policy.

## Consequences

- Positive: game logic can consume logical actions independent of raw HID layouts.
- Positive: multi-controller assignment is now explicit and testable.
- Trade-off: no runtime UI workflow for remapping/ownership/context yet.

## Alternatives considered

- **Hard-code platform button ids in gameplay:** rejected due to cross-platform maintenance cost.
- **Full production input action system now:** deferred to keep chunk scope incremental and testable.
