# ADR-0011: Baseline single-thread loop and explicit parallelism hints

## Status

Accepted

## Context

Chapter 4 introduces parallelism and explicit parallel work. Marble already committed to a **deterministic single-thread main loop** ([ADR-0001](ADR-0001-deterministic-single-thread-runtime-loop.md)) and exposes OS **hardware concurrency** and **page size** hints ([ADR-0009](ADR-0009-hardware-query-surface.md)). Chunk **011** is the right place to make **thread-spawning policy** and **future worker headroom** explicit in code, without building a job system ([`OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) concurrency row remains open until **011** / **014**).

## Decision

1. **`engineSpawnsWorkerThreads()`** in [`engine/core/Parallelism.hpp`](../../engine/core/Parallelism.hpp) returns **`false`** for this milestone: `core::Engine` does not create `std::thread` or platform worker pools for frame phases.
2. **`maxRecommendedWorkerThreads()`** in [`engine/core/Parallelism.cpp`](../../engine/core/Parallelism.cpp) returns **`logicalProcessorCount() - 1`** when that count is **> 1**, else **0**. Rationale: reserve one logical CPU for the main thread and OS; when `logicalProcessorCount()` is **0** (unknown), return **0** headroom.
3. **Explicit parallelism** (worker threads, job queues, task graphs) will be **opt-in** later and must not break deterministic stepping contracts where they apply ([ADR-0003](ADR-0003-fixed-step-simulation-phase.md)).
4. **Relationship to ADR-0010**: `CacheLinePad` and cache-line awareness apply when shared mutable state crosses threads; still no workers today.

## Consequences

- Positive: a single place to read “does the engine spawn threads?” and a conservative **numeric hint** for future pools.
- Positive: tests tie `maxRecommendedWorkerThreads` to `logicalProcessorCount` without introducing threads in CI.
- Trade-off: the “minus one” rule is heuristic; IO-heavy or GPU-driven engines may tune differently later.

## Alternatives considered

- **Spawn a worker thread now to validate plumbing:** rejected — violates ADR-0001 without a job API or clear ownership.
- **Return `logicalProcessorCount()` with no reserve:** rejected as a default — risks starving the main thread on small-core machines.
