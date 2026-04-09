# ADR-0039: Collision detection — raycasts and naive broad-phase pairs (Chapter 13 §13.3)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 039** in **§13.3 The Collision Detection System**. Building on middleware primitives ([`ADR-0038`](ADR-0038-collision-middleware-primitives-and-filters.md)), the engine needs **query paths** (ray versus primitives) and a **minimal broad-phase seam** that can be replaced by grids, trees, or middleware later.

## Decision

1. Add `[physics/CollisionDetection.hpp](../../engine/physics/CollisionDetection.hpp)`:
   - `rayAabbInterval` / `raycastAabb` — slab clipping for axis-aligned boxes; `raycastAabb` returns the first `t >= 0` along `Ray3::directionUnit`,
   - `raycastSphere` — analytic ray–sphere intersection (nearest forward `t`),
   - `BroadphaseProxy` (`id` + `Aabb`) and `collectAabbOverlapPairs` — **O(n²)** unordered overlap enumeration with a hard `maxPairs` cap (documented baseline, not a shipping broad-phase).
2. Verify with `collision_detection_test`.

## Consequences

- Positive: gameplay queries (weapons, clicks, sensors) and tests can run without a third-party physics SDK.
- Positive: pair collection is an explicit seam to swap for spatial acceleration in a later chunk or integration ADR.
- Trade-off: no contact manifolds, GJK, or continuous collision detection yet.
- Follow-up: sweep-and-prune, BVH/grid, persistent manifolds, and physics engine delegation.

## Alternatives considered

- **Only middleware ray APIs:** rejected; we still want deterministic, dependency-free collision detection tests in CI.
