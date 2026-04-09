# ADR-0015: Lock-free primitives and atomic memory order

## Status

Accepted

## Context

Chapter 4.9 covers **lock-free** algorithms and **atomic** operations with **memory ordering** constraints. Marble already has mutex-based vocabulary ([ADR-0013](ADR-0013-thread-synchronization-vocabulary.md)) and lock **ordering** rules ([ADR-0014](ADR-0014-lock-ordering-and-deadlock-avoidance.md)). We need a **documented** default for **`std::atomic`** use and a thin **type alias** surface before introducing queues, job stealers, or networking buffers.

## Decision

1. Centralize `std::atomic` behind [`Atomic`, `AtomicU32`, `AtomicU64`](../../engine/core/Atomics.hpp) in [`engine/core/Atomics.hpp`](../../engine/core/Atomics.hpp).
2. **Memory order defaults:**
   - **`memory_order_seq_cst`**: use when atomic operations must **publish** state to other threads without a separate mutex (simplest mental model).
   - **`memory_order_relaxed`**: allowed for **independent counters and statistics** where no other data is synchronized through the atomic (see [`lock_free_primitives_test`](../../tests/lock_free_primitives_test.cpp)).
   - **`acquire` / `release`**: use for **handoff** patterns (producer publishes pointer or generation; consumer observes); pair them explicitly in code comments at the site.
3. **Contended atomics** on hot paths should consider [`CacheLinePad`](../../engine/core/MemoryArchitecture.hpp) between unrelated atomics ([ADR-0010](ADR-0010-memory-architecture-conventions.md)).
4. **Lock-free queues, stacks, and hazard pointers** are **out of scope** until a consumer subsystem needs them; add with profiling and tests.

## Consequences

- Positive: one place to grep for atomic usage and tighten policy later.
- Positive: `lock_free_primitives_test` validates relaxed increments under thread contention.
- Trade-off: aliases are not enforced by the compiler; review still required for raw `std::atomic` in new code.

## Alternatives considered

- **Always `seq_cst`:** rejected — too expensive for hot statistics on some platforms.
- **Implement a lock-free queue now:** rejected — no second call site yet; premature complexity.
