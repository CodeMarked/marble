# ADR-0008: Data and memory layout conventions (no allocator yet)

## Status

Accepted

## Context

Chapter 3.3 covers how data is laid out in memory: alignment, padding, structure layout, and separation of concerns between “hot” and “cold” data. Marble does not yet have a custom allocator or full resource system ([ADR-0004](ADR-0004-runtime-assets-root.md) covers runtime file roots only). We still need **shared vocabulary** and **small helpers** so subsystems do not invent incompatible alignment rules.

## Decision

1. Provide **`marble::core::alignUp`**, **`alignDown`**, and **`isPowerOfTwo`** in [`engine/core/MemoryLayout.hpp`](../../engine/core/MemoryLayout.hpp) for size-based rounding. Callers must pass a **power-of-two** alignment; behavior is otherwise undefined (documented in the header).
2. Publish a single conventional **`kCacheLineBytes` (64)** for padding and false-sharing notes. It is a **portable default**, not a CPUID query; specialized code may use a different value locally.
3. Prefer **explicit layout** (`alignas`, ordering of members) for performance-sensitive structs as they are introduced; avoid reliance on “works on my compiler” padding.
4. **SoA vs AoS** and **allocator strategy** remain open until gameplay and resource batches exist; revisit around book **chunk 020** (memory management) — see [`docs/notes/OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) data-ownership row.

## Consequences

- Positive: one place for alignment math used by future buffers, pools, and job payloads.
- Positive: tests lock in constexpr behavior across platforms.
- Trade-off: `kCacheLineBytes` may not match every CPU; call sites that care must measure or parameterize.
- Follow-up: custom allocators, SIMD alignment, and pointer alignment helpers belong with later memory chapters ([ADR-0007](ADR-0007-software-engineering-invariants.md) applies to invariants, not layout math).

## Alternatives considered

- **Pull in a full alignment / SIMD library now:** rejected as premature before first heavy workloads.
- **Query cache line size at runtime:** deferred; adds platform code without a consumer yet.
