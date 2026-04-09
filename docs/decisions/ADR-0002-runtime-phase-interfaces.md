# ADR-0002: Runtime phase interfaces for simulation and rendering

## Status

Accepted

## Context

Chunk 002 emphasizes reusable engine architecture, coarse-grained subsystem patterns, and the blurred but important boundary between generic engine systems and game-specific logic. Marble's runtime loop previously contained simulation and render placeholders directly in `core::Engine`, with no explicit integration seam.

## Decision

Introduce explicit runtime phase interfaces owned by `core::Engine`:

- `ISimulationPhase` with `tick(FrameContext)`.
- `IRenderPhase` with `render(FrameContext)`.
- Runtime dispatches these interfaces each frame in deterministic order.
- Provide default no-op implementations to preserve baseline behavior.

## Consequences

- Positive: decouples orchestration from subsystem implementations.
- Positive: enables gradual integration of simulation/render systems without changing lifecycle core.
- Positive: keeps core runtime reusable across game modes and future platform targets.
- Trade-off: one additional abstraction layer and interface management overhead.

## Alternatives considered

- Keep simulation/render as direct hard-coded calls in `Engine`: rejected due to weak extensibility.
- Add full plugin/service locator architecture now: rejected as premature complexity.

## Follow-up actions

- First concrete simulation implementation: `FixedStepSimulationPhase` (see ADR-0003) and `fixed_step_simulation_test`.
- Implement first concrete render phase bridge and frame output contract.
- Define server-mode simulation phase variant for future authoritative hosts.
