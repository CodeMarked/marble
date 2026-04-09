# ADR-0014: Lock ordering and deadlock avoidance (rules, not a detector)

## Status

Accepted

## Context

Chapters 4.7–4.8 cover classic lock-based pitfalls: **deadlock**, **priority inversion** (conceptually), **lock ordering** violations, and rules of thumb for concurrent code. Marble already centralizes mutex vocabulary ([ADR-0013](ADR-0013-thread-synchronization-vocabulary.md)) and keeps the frame loop worker-free by default ([ADR-0011](ADR-0011-baseline-single-thread-and-explicit-parallelism-hints.md)). We still need an explicit, **documented** lock **hierarchy** before multiple subsystems acquire more than one mutex in nested scopes.

## Decision

1. Introduce [`LockLevel`](../../engine/core/LockOrdering.hpp) and [`mayAcquireAfter`](../../engine/core/LockOrdering.hpp) in [`engine/core/LockOrdering.hpp`](../../engine/core/LockOrdering.hpp): a **total order** on named domains. When a thread holds a mutex **associated** with level `held`, it may acquire another mutex at level `next` only if `mayAcquireAfter(held, next)` is **true** (strictly increasing level).
2. **Do not** acquire locks in reverse order relative to this enum without a **separate ADR** or subsystem-specific proof (for example lock-free handoff).
3. **RecursiveMutex** ([ADR-0013](ADR-0013-thread-synchronization-vocabulary.md)) is for **re-entrancy within one level**, not for bypassing ordering between **distinct** mutexes at different levels.
4. **No automated deadlock detector** in this milestone; rely on code review, consistent ordering, and keeping critical sections small.
5. **Holding locks while calling** into **unknown** or **user** code is discouraged; document exceptions per subsystem when unavoidable.

## Consequences

- Positive: a concrete rule engineers can apply in reviews (“is the second lock higher than the first?”).
- Positive: `lock_ordering_test` locks the ordering predicate at compile time.
- Trade-off: `LockLevel` must evolve as real mutexes appear; stale enums are misleading—update this ADR when adding levels.

## Alternatives considered

- **Runtime lock graph / deadlock detector:** deferred — cost and integration complexity; revisit with a threading runtime.
- **No enum, docs-only:** rejected — too easy to ignore under schedule pressure.
