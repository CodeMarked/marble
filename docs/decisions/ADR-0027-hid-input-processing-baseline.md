# ADR-0027: HID input processing baseline (Chapter 9 through §9.5.4.1)

## Status

Accepted

## Context

Chunk 027 transitions into Human Interface Devices and engine-facing input processing concerns: dead zones, analog signal filtering, and button up/down event detection from packed button bits.

Marble had no dedicated HID processing helpers yet.

## Decision

1. Add `[input/Hid.hpp](../../engine/input/Hid.hpp)` as a header-only baseline containing:
  - `ButtonStateTracker` for current/previous button words and up/down event derivation via XOR/masking.
  - `applyCenteredDeadZone()` and `applyPositiveDeadZone()` helpers.
  - `lowPassFilter()` using the first-order discrete RC formula.
  - `MovingAverage<T, N>` utility for simple smoothing windows.
2. Keep this layer platform-agnostic and independent from GLFW/XInput details.
3. Treat chord support as "all masked buttons currently down" only in this chunk; advanced timing windows remain deferred.

## Consequences

- Positive: engine-side input normalization and event extraction become explicit and testable.
- Positive: future platform adapters can emit raw samples while gameplay consumes normalized values.
- Trade-off: no device discovery/remapping/multi-player routing yet.

## Alternatives considered

- **Raw input passthrough only:** rejected; would push noise/dead-zone/event logic into gameplay code.
- **Full HID abstraction (devices, remapping, contexts) now:** deferred to later input-system chunks.

