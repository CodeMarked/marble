# ADR-0009: Minimal hardware query surface (not a HAL)

## Status

Accepted

## Context

Chapter 3.4 introduces processors, memory hierarchies, buses, and how physical constraints influence engine design. Marble still runs a **deterministic single-thread baseline** ([ADR-0001](ADR-0001-deterministic-single-thread-runtime-loop.md)), but subsystems will eventually need **factual signals** from the OS (page granularity, hardware concurrency hints) without inventing per-feature queries.

[`ADR-0008`](ADR-0008-data-and-memory-layout-conventions.md) defines a **conventional** `kCacheLineBytes` (64) because cache geometry is not yet queried from the CPU.

## Decision

1. Add [`engine/platform/hardware/System.hpp`](../../engine/platform/hardware/System.hpp) with:
   - **`memoryPageSizeBytes()`** — OS-reported virtual page size (`GetSystemInfo` on Windows, `sysconf(_SC_PAGESIZE)` elsewhere, with a **4096** fallback if the POSIX query fails).
   - **`logicalProcessorCount()`** — `std::thread::hardware_concurrency()`; **0** means unknown or not representable (standard library contract).
2. Keep implementation in **`marble::platform`** next to other OS-facing helpers (see [`ExecutableDir`](../../engine/platform/paths/ExecutableDir.cpp)).
3. Do **not** add CPUID, cache-size enumeration, or NUMA topology in this milestone; those belong to later performance and parallelism chunks with profiling evidence.
4. Relationship to **ADR-0008**: page size is **queried**; cache line size remains a **documented default** until a measured policy is justified.

## Consequences

- Positive: allocators and buffer policies can align to **real** page boundaries when needed.
- Positive: job systems can read a **standard** concurrency hint without pulling in a threading framework early.
- Trade-off: `logicalProcessorCount() == 0` must be handled by future parallel code; the baseline loop remains single-threaded.
- Follow-up: optional cache-line / core topology queries when SIMD and job dispatch land ([ADR-0008](ADR-0008-data-and-memory-layout-conventions.md) follow-ups).

## Alternatives considered

- **Embed everything in `MemoryLayout.hpp`:** rejected — page size is platform/OS, not pure layout math.
- **CPUID for cache line now:** rejected — premature without consumers and complicates CI matrices.
