# ADR-0028: HID chords and gesture detection baseline (Chapter 9 §9.5.4.2–§9.5.4.3)

## Status

Accepted

## Context

After landing dead zones/filtering/button up-down events in ADR-0027, the next chapter slice introduces:

- multi-button chords with human timing tolerance,
- time-bounded gesture/sequence detection (e.g., A-B-A),
- rapid button tapping detection.

## Decision

1. Extend `[input/Hid.hpp](../../engine/input/Hid.hpp)` with:
  - `ChordDetector` (frame-grace window, single emit while held),
  - `ButtonTapDetector` (rapid-tap validity from inter-press delta),
  - `ButtonSequenceDetector` (ordered button mask sequence with max total duration).
2. Keep implementations header-only and allocation-free for deterministic low-overhead use.
3. Keep sequence/chord semantics strict by default (unexpected button-down resets sequence).

## Consequences

- Positive: gameplay can consume reusable gesture primitives rather than re-implementing timing logic per feature.
- Positive: chord/sequence behavior is now deterministic and unit-tested.
- Trade-off: this baseline does not yet include thumb-stick rotation gestures, context ownership, or full remapping.

## Alternatives considered

- **Implement only simple `chordDown` bit-check:** rejected because it does not handle near-simultaneous human input.
- **Full input action/state machine layer now:** deferred to later HID chunks to keep scope incremental.

