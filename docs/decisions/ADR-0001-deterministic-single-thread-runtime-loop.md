# ADR-0001: Deterministic single-thread runtime loop baseline

## Status

Accepted

## Context

Marble needs a reliable engine-first baseline before adding rendering complexity, multiplayer networking, or concurrency features. The existing executable loop only polled window events and did not enforce explicit lifecycle contracts or frame phase ownership.

## Decision

Adopt a deterministic single-thread runtime baseline where `core::Engine` owns lifecycle and frame phase ordering:

- `init`: create platform/runtime prerequisites and fail fast on errors.
- `run`: execute ordered per-frame phases (input/events, simulation placeholder, render placeholder).
- `shutdown`: release resources in dependency-safe order.

Concurrency and multiprocess frame execution are deferred until later chunks focused on parallelism and scheduling.

## Consequences

- Positive: simpler debugging, stable startup/run/shutdown behavior, and explicit ownership boundaries.
- Positive: provides a clean integration point for future simulation, rendering, and networking systems.
- Trade-off: no parallel frame work yet; throughput scalability deferred.
- Constraint: future concurrency work must preserve deterministic behavior where practical.

## Alternatives considered

- Keep lifecycle orchestration in `main`: rejected due to weak subsystem boundaries.
- Introduce job system now: rejected because architecture and ownership contracts are still being established.

## Follow-up actions

- Simulation and rendering phase interfaces are defined in ADR-0002; default simulation timing in ADR-0003.
- Add frame timing metrics and diagnostics sinks beyond stdout logging (partial: periodic FPS log in engine diagnostics; extend as needed).
- Revisit scheduling model during concurrency-related book chunks.
