# ADR-0026: Multiprocessor loop seams and inline job baseline (Chapter 8 §8.6)

## Status

Accepted

## Context

Chunk 026 covers multiprocessor loop decomposition patterns: subsystem-threading, scatter/gather, and general-purpose job systems.

Marble already has an explicit policy of no engine worker threads in this milestone (`ADR-0011`), but it lacked a concrete API seam for job declarations, counters, and decomposition.

## Decision

1. Add [`core::job`](../../engine/core/JobSystem.hpp) baseline types:
   - `EntryPoint`, `Priority`, `Declaration`, and `Counter`.
2. Add `InlineJobSystem` with `kickJob(s)`, `waitForCounter`, and `kick...AndWait` APIs.
3. Add `scatterGather(total, batch, fn)` helper for explicit batch decomposition.
4. Execute jobs inline on the caller thread for now (no worker pool), preserving `ADR-0011`.

## Consequences

- Positive: call sites can adopt job/scatter-gather structure now without introducing concurrency risk.
- Positive: future worker-thread/job-queue implementation can replace internals while preserving API shape.
- Trade-off: no parallel speedup yet; this is architecture scaffolding.

## Alternatives considered

- **One thread per subsystem now:** rejected as too rigid and contrary to current policy.
- **Ship thread pool/job queue now:** deferred until profiling evidence and runtime integration justify complexity.
