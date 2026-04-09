# ADR-0013: Thread synchronization vocabulary (lock-based baseline)

## Status

Accepted

## Context

Chapters 4.5–4.6 introduce concurrent programming concepts and **synchronization primitives** (mutexes, condition variables, and related patterns). Marble already keeps the **engine frame loop** free of worker threads ([ADR-0011](ADR-0011-baseline-single-thread-and-explicit-parallelism-hints.md)) but subsystems, loaders, and future job code will need **shared, documented** locking vocabulary.

## Decision

1. Centralize standard-library lock types in [`engine/core/SyncPrimitives.hpp`](../../engine/core/SyncPrimitives.hpp):
   - **`Mutex`** → `std::mutex` (default).
   - **`RecursiveMutex`** → `std::recursive_mutex` (use sparingly; document re-entrancy when chosen).
   - **`LockGuard`**, **`UniqueLock`** templates over `std::lock_guard` / `std::unique_lock`.
   - **`ConditionVariable`** → `std::condition_variable`.
2. Prefer **`Mutex` + `LockGuard`** for short critical sections; use **`UniqueLock`** when a **`ConditionVariable`** is involved.
3. **Atomics** (`std::atomic`) remain valid without a wrapper; document **memory order** at use sites (see chunk **015** for lock-free).
4. **Tests** may spawn `std::thread` to validate primitives (see [`synchronization_primitives_test`](../../tests/synchronization_primitives_test.cpp)); production code **still** does not spawn engine worker threads until policy changes ([ADR-0011](ADR-0011-baseline-single-thread-and-explicit-parallelism-hints.md)).
5. CMake tests that use `std::thread` link **`Threads::Threads`** ([`tests/CMakeLists.txt`](../../tests/CMakeLists.txt)).

## Consequences

- Positive: one include for mutex/condvar naming; future instrumentation (tracing, deadlock checks) has a single seam.
- Positive: `synchronization_primitives_test` proves mutex + `LockGuard` under contention.
- Trade-off: aliases add indirection for readers; keep them thin typedefs only.

## Alternatives considered

- **Raw `std::mutex` everywhere:** rejected — scatters policy and complicates future replacement or profiling.
- **Implement a custom mutex now:** rejected — no measured contention yet; defer until profiling demands it.
