# ADR-0038: Collision middleware primitives and layer filters (Chapter 13 §13.1–§13.2)

## Status

Accepted

## Context

`[docs/book/chunks/index.md](../book/chunks/index.md)` places **chunk 038** in **Chapter 13 — Collision and Rigid Body Dynamics** through **§13.2 Collision/Physics Middleware**. Before wiring a third-party physics engine (chunk 041+), the engine needs **narrow/broad-phase style predicates** and a **filtering model** that matches how middleware pairs bodies.

## Decision

1. Add `[physics/CollisionMiddleware.hpp](../../engine/physics/CollisionMiddleware.hpp)` under `marble::physics`:
  - `intersects(Sphere, Sphere)` and `intersects(Aabb, Aabb)` using existing `[math/Geometry.hpp](../../engine/math/Geometry.hpp)` types,
  - `CollisionFilter` with `membershipLayers` and `collideAgainstMask`,
  - `filtersAllow` implementing **symmetric** pairing: both `(a.membership & b.mask)` and `(b.membership & a.mask)` must be non-zero.
2. Verify with `physics_middleware_test`.

## Consequences

- Positive: gameplay and tools can share one vocabulary for “what collides with what” before Bullet/Havok/etc.
- Positive: pure `constexpr` overlap tests stay fast and deterministic in CI.
- Trade-off: no broad-phase structures, contact manifolds, or engine handles yet (chunk 039+).
- Follow-up: sweep tests, ray casts, trigger volumes, and actual middleware adapter types.

## Alternatives considered

- **Putting overlaps only in `math/Geometry.hpp`:** rejected to keep `math` free of physics-layer policy; `physics/` owns middleware-facing collision policy.

