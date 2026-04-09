# ADR-0003: Fixed-step simulation as runtime default

## Status

Accepted

## Context

Chunk 003 emphasizes layered runtime architecture, strict dependency direction, and real-time simulation requirements. Marble already had an interface seam for simulation and rendering, but simulation defaulted to a no-op implementation, leaving timing behavior undefined.

## Decision

Adopt a fixed-step simulation phase (`FixedStepSimulationPhase`) as the default runtime simulation implementation:

- Use a configurable fixed timestep (default `1/60` second).
- Accumulate variable frame delta and execute bounded substeps each frame.
- Cap substeps per frame and drop excess accumulated time to avoid runaway catch-up loops.
- Keep simulation implementation behind `ISimulationPhase` to preserve loop/subsystem decoupling.

## Consequences

- Positive: deterministic-friendly baseline simulation semantics.
- Positive: clear separation between runtime orchestration and simulation logic.
- Positive: testable stepping behavior independent of window/render systems.
- Trade-off: dropped time can reduce temporal accuracy during severe frame stalls.

## Alternatives considered

- Keep no-op simulation as default: rejected because it does not define runtime simulation behavior.
- Variable-step simulation default: rejected due to lower determinism and integration risk.
- Full job-system simulation now: rejected as premature before core subsystem hardening.

## Follow-up actions

- Add configurable policy for stall handling (drop-time vs bounded catch-up).
- Define first gameplay-foundation simulation workload and wire it through fixed-step callbacks.
- Add profiling counters for simulation substeps and dropped batches to diagnostics output.
