# ADR-0021: Container baseline with fixed-capacity closed hash table (Chapter §6.3)

## Status

Accepted

## Context

Chunk 021 covers `§6.3 Containers`, including operations/complexity trade-offs and dictionary implementations via hash tables. The text calls out **closed hash tables** as attractive in console/runtime contexts because they use fixed memory and avoid per-insert dynamic allocation.

Marble has foundational math and memory pieces but did not yet have a runtime-oriented dictionary container primitive.

## Decision

1. Add [`core::ClosedHashTable`](../../engine/core/ClosedHashTable.hpp) as a fixed-capacity dictionary container.
2. Use **closed hashing with linear probing**:
   - key/value pairs stored directly in slots,
   - collision resolution by probing subsequent slots,
   - no dynamic allocation during `insertOrAssign`.
3. Keep API minimal and explicit:
   - `insertOrAssign`, `find`, `erase`, `clear`, `size`, `capacity`.
4. Preserve this as a baseline building block, not a full STL replacement.
5. Defer advanced variants (quadratic probing, Robin Hood hashing, custom allocator-backed generic containers) to later chunks/needs.

## Consequences

- Positive: predictable memory footprint and allocation behavior for dictionary-style runtime data.
- Positive: performance characteristics are straightforward to reason about (`O(1)` expected lookup/insert with good hashing, degraded under heavy clustering).
- Trade-off: fixed capacity and linear probing can degrade at high load factors; resizing strategy is intentionally deferred.

## Alternatives considered

- **Use only `std::unordered_map`:** deferred as default runtime policy because chunk guidance emphasizes tighter control over allocation and memory behavior.
- **Implement open hash table with chained nodes:** rejected for baseline because it requires dynamic allocations on collisions.
