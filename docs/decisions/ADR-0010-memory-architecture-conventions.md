# ADR-0010: Memory architecture conventions (cache lines, not cache sizes)

## Status

Accepted

## Context

Chapter 3.5 discusses memory hierarchies (registers → cache levels → RAM), cache coherence, and how layout affects performance. Marble already has:

- [`ADR-0008`](ADR-0008-data-and-memory-layout-conventions.md): alignment math and a **conventional** `kCacheLineBytes` (not CPUID-measured).
- [`ADR-0009`](ADR-0009-hardware-query-surface.md): OS **page size** and **logical processor count**; explicitly **no** CPUID or NUMA topology yet.

We still need a **code-level hook** for the classic “memory architecture” mitigation: **false sharing** between cores when unrelated mutable data lands in the same cache line.

## Decision

1. Introduce [`CacheLinePad`](../../engine/core/MemoryArchitecture.hpp) in [`engine/core/MemoryArchitecture.hpp`](../../engine/core/MemoryArchitecture.hpp): one cache-line-sized, cache-line-aligned opaque block for separating hot fields.
2. Treat **L1/L2/L3 capacity and associativity** as **profiling- and platform-specific** facts; do **not** bake numeric cache sizes into engine headers without measurement hooks and consumers.
3. **NUMA** is **not** assumed for the baseline single-threaded runtime ([`ADR-0001`](ADR-0001-deterministic-single-thread-runtime-loop.md)); when job systems land, revisit affinity and NUMA with evidence ([`OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) concurrency row, gate **011** / **014**).
4. Keep **page-aware** allocation policies tied to [`memoryPageSizeBytes()`](../../engine/platform/hardware/System.hpp) from ADR-0009; keep **line-aware** layout tied to `kCacheLineBytes` and `CacheLinePad`.

## Consequences

- Positive: a single named type for a well-known engine pattern, test-locked alignment/size.
- Positive: avoids fake “portable” L2 size constants that are wrong on most machines.
- Trade-off: `kCacheLineBytes` remains conventional until a query path is justified ([ADR-0008](ADR-0008-data-and-memory-layout-conventions.md) follow-ups).

## Alternatives considered

- **Emit L1/L2/L3 sizes as `constexpr`:** rejected without CPUID or stable cross-platform source.
- **Merge `CacheLinePad` into `MemoryLayout.hpp`:** rejected to keep “layout math” separate from “hierarchy / sharing patterns.”
