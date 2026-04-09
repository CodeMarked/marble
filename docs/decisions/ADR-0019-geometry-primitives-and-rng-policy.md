# ADR-0019: Geometry primitives and RNG policy (Chapter §5.5–§5.7)

## Status

Accepted

## Context

Chunk 019 covers:

- **§5.5** comparison of rotational representations and practical trade-offs.
- **§5.6** common math objects used in engine code (lines/rays/segments, spheres, planes, AABB, frusta).
- **§5.7** pseudorandom number generation quality/performance considerations.

After `Vec3`/`Mat4` (ADR-0017/0018), Marble needs lightweight geometric building blocks and a default RNG stance that matches the text's guidance.

## Decision

1. Add geometry primitives in `[engine/math/Geometry.hpp](../../engine/math/Geometry.hpp)`:
  - `Line3`, `Ray3`, `Segment3`, `Sphere`, `Plane`, `Aabb`, and `Frustum` (`std::array<Plane, 6>`).
2. Encode the plane in compact `[n d]` form (`dot(n, p) + d = 0`) and expose `signedDistance(plane, point)` per §5.6.3.
3. Provide minimal operations that map directly to §5.6 formulas and tests:
  - `pointOnLine`, `pointOnRay`, `pointOnSegment`
  - `contains(Sphere, Point3)` (distance-squared)
  - `contains(Aabb, Point3)` (axis range checks)
  - `contains(Frustum, Point3)` via six plane half-space tests.
4. Add RNG policy in `[engine/math/Random.hpp](../../engine/math/Random.hpp)`:
  - Default engine alias `Rng = std::mt19937` (Mersenne Twister family, §5.7.2).
  - Keep `Lcg32` available only as an explicit fast/legacy option (§5.7.1 quality caveats).
  - Provide bounded helpers: `uniform01`, `uniformRange`, `uniformU32`.
5. Defer axis-angle/quaternion/dual-quaternion APIs and rotation blending choices to a future animation/camera chunk; this ADR records representational trade-offs from §5.5 but does not add new rotation types.

## Consequences

- Positive: engine math now includes commonly needed collision/culling primitives without pulling third-party math dependencies.
- Positive: RNG behavior is deterministic-by-seed and has a clear quality default.
- Trade-off: frustum support is point-only; object/frustum and broad-phase optimizations remain future work.

## Alternatives considered

- **Custom full PRNG implementation now (PCG/Xorshift/KISS):** deferred; standard-library MT keeps scope small and testable.
- **Implement quaternion/SRT/dual-quaternion immediately:** deferred because no animation system is landed yet.

