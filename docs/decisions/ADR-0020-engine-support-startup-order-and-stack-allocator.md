# ADR-0020: Engine support start-up order and stack allocator baseline (Chapter 6 through §6.2.1.1)

## Status

Accepted

## Context

Chunk 020 covers Chapter 6 intro through early memory management:

- **§6.1** argues for explicit subsystem start-up/shut-down ordering and warns against relying on C++ global/static construction order.
- **§6.2.1.1** introduces stack-based allocators with marker/rollback semantics for fast, predictable allocation behavior.

Marble already uses an engine-owned runtime lifecycle (`init -> run -> shutdown`), but had no concrete stack allocator utility yet.

## Decision

1. Keep the engine’s explicit lifecycle ownership model (`core::Engine`) as the concrete realization of §6.1 ordering guidance; do not introduce construct-on-demand singleton managers.
2. Add `[core::StackAllocator](../../engine/core/StackAllocator.hpp)` as a baseline custom allocator:
  - Preallocated contiguous storage.
  - Bump allocation from stack top.
  - Marker API (`getMarker`, `freeToMarker`) and `clear`.
  - Alignment-aware allocation (`alloc(size, alignment)`).
3. Preserve a strict policy aligned with §6.2:
  - Avoid heap allocation in tight loops when feasible.
  - Use stack allocator only where LIFO/phase rollback fits usage.
4. Defer double-ended stacks, pool allocators, and general-purpose heap replacement to later memory chunks.

## Consequences

- Positive: introduces a low-overhead allocator primitive that is deterministic and easy to reason about.
- Positive: marker rollback supports frame/phase scratch patterns and level-scope temp allocations.
- Trade-off: cannot free arbitrary blocks; misuse of markers can violate intended LIFO discipline.

## Alternatives considered

- **Keep only global `new`/`delete`:** rejected as the sole strategy because it does not address chapter guidance for predictable, low-overhead hot-path allocations.
- **Implement full allocator suite now (pool, buddy, TLSF):** deferred to keep chunk scope tight and testable.

