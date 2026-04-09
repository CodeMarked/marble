# ADR-0025: Time measurement and frame-delta policy baseline (Chapter 8 through §8.5)

## Status

Accepted

## Context

Chunk 025 covers game-loop timing, frame-rate/delta behavior, high-resolution clocks, running-average smoothing, and breakpoint spike handling.

Marble’s loop measured `dt` directly each frame, but lacked a centralized policy for smoothing or debugger-spike protection.

## Decision

1. Add [`core::HiResClock`](../../engine/core/Time.hpp) as a monotonic high-resolution timing wrapper.
2. Add [`core::FrameDeltaEstimator`](../../engine/core/Time.hpp) implementing:
   - short running-average smoothing over measured frame deltas,
   - clamp of unusually large deltas (breakpoint-resume spikes) to target frame delta,
   - non-negative sanitization.
3. Wire `Engine::run()` to pass measured wall-clock `dt` through this estimator before simulation/render phases.
4. Keep frame-rate governing (`sleep`/v-sync lock) and advanced multiprocessor/job-loop strategies out of scope for this chunk.

## Consequences

- Positive: reduced sensitivity to transient frame spikes and debugger pauses.
- Positive: timing policy is now explicit, testable, and reusable.
- Trade-off: smoothing introduces slight temporal lag versus raw per-frame measurements.

## Alternatives considered

- **Use raw measured `dt` only:** rejected due to spike instability and debugger-resume hazards.
- **Hard frame lock/governor now:** deferred until rendering/v-sync policy is landed.
