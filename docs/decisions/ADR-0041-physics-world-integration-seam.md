# ADR-0041: Physics world integration seam (Chapter 13 §13.5–§13.6)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 041** in **§13.5 Integrating a Physics Engine into Your Game** through **§13.6 Advanced Physics Features**. The book discusses wiring a middleware world, stepping it from the game loop, and advanced options such as sub-stepping and continuous collision. Marble stays dependency-free for this milestone but needs a **narrow, testable seam** where a real SDK would plug in later.

## Decision

1. Add [`physics/PhysicsIntegration.hpp`](../../engine/physics/PhysicsIntegration.hpp):
   - `PhysicsWorldSettings` — `gravity` (default `0, -9.81, 0`), `enableContinuousCollision`, `maxSubSteps` (reserved for middleware; CCD is intentionally unused in `step` until wired).
   - `SimplePhysicsWorld` — `setSettings` / `settings` / `step(deltaSeconds, RigidBodyKinematics*, bodyCount)`; splits time by `maxSubSteps` and calls [`integrateSemiImplicitEuler`](../../engine/physics/RigidBodyDynamics.hpp) each sub-step.
   - `rigidBodyTranslationMatrix` — translation-only `Mat4` for gameplay/render sync ([`ADR-0018`](ADR-0018-mat4-and-transforms.md)).
2. Verify with `physics_world_integration_test`.

## Consequences

- Positive: game code can own a single “world” object and settings without committing to Bullet/Rapier yet.
- Positive: sub-step count is exercised in tests; CCD flag round-trips for future middleware mapping.
- Trade-off: no actual continuous collision, constraints, or SDK serialization.
- Follow-up: replace `step` body with middleware calls; map CCD and solver tolerances from `PhysicsWorldSettings` or a richer config type. See [`ADR-0058`](ADR-0058-physics-middleware-integration.md) (`IPhysicsWorld`, optional Jolt smoke).

## Alternatives considered

- **Vendor SDK in this chunk:** rejected; keep the tree buildable without physics middleware.
- **No world type — call `integrateSemiImplicitEuler` only from gameplay:** rejected; the chapter emphasizes an engine/world boundary; a small type preserves that contract.
