# ADR-0040: Rigid body dynamics integration baseline (Chapter 13 §13.4)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 040** in **§13.4 Rigid Body Dynamics**. With collision queries ([`ADR-0039`](ADR-0039-collision-detection-raycast-and-naive-broadphase.md)), the runtime needs a **small, deterministic integration step** for linear motion and a minimal rotational degree of freedom before full solver coupling.

## Decision

1. Add `[physics/RigidBodyDynamics.hpp](../../engine/physics/RigidBodyDynamics.hpp)`:
   - `RigidBodyKinematics` (`position`, `linearVelocity`, `invMass`; `invMass == 0` means no integration),
   - `integrateSemiImplicitEuler` — `v += a dt`, then `x += v dt`,
   - `applyImpulseLinear` — `Δv = J * invMass`,
   - `RigidBodyYaw` + `integrateYawAxis` — scalar **+Y** torque integration (`α = τ * invI`, then `ω`, `θ` updates) aligned with ADR-0017 up axis.
2. Verify with `rigid_body_dynamics_test`.

## Consequences

- Positive: simulation phases can advance simple bodies without a third-party dynamics SDK.
- Positive: inverse mass / inverse inertia zeros provide a stable “static” representation.
- Trade-off: no quaternion state, no gyroscopic coupling, no constraint solver or contact resolution yet.
- Follow-up: orientation in quaternions, inertia tensors, sleeping, and joint/contact impulses.

## Alternatives considered

- **Full Bullet/Rapier integration in this chunk:** deferred; keep policy testable and dependency-free.
- **Explicit Euler only:** rejected; semi-implicit Euler is the common game-engine default for stability at moderate `dt`.
