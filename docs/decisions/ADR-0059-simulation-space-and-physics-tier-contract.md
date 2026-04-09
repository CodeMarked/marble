# ADR-0059: Simulation space and physics tier contract

## Status

Accepted

## Context

Gameplay and rendering use **`marble::math::Vec3` (float)** ([`ADR-0017`](ADR-0017-three-d-math-conventions-and-vec3.md)). Rigid-body snapshots use [`RigidBodyKinematics`](../../engine/physics/RigidBodyDynamics.hpp). Middleware-backed bodies use [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp) ([`ADR-0058`](ADR-0058-physics-middleware-integration.md)). Planet-scale worlds and multiplayer-first play ([`ADR-0054`](ADR-0054-online-multiplayer-authority-topology-baseline.md)) need a **single documented contract** for what “world position” means at the physics seam, and **rules** for future tier handoffs so precision and authority stay coherent. [`ADR-0050`](ADR-0050-world-chunk-data-object-references-and-queries-baseline.md) defines chunk **data** layout; it does not yet imply a physics-local frame.

## Decision

1. **Simulation space today**  
   Gameplay and layout use **simulation-local** [`Vec3`](../../engine/math/Vec3.hpp). Garden maps into [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp) via [`SimulationIsland`](../../engine/gameplay/SimulationIsland.hpp) (double anchor + `localGameplayToJolt` / static helpers). With **default anchor zero**, Jolt still sees the same floats as before. The **middleware** itself does not rebase; the game owns the mapping. There is **no automatic floating rebasing** of the anchor yet.

2. **Who owns pose each frame**  
   Before [`step`](../../engine/physics/IPhysicsScene.hpp), [`syncHostVelocitiesBeforeStep`](../../engine/physics/IPhysicsScene.hpp) pushes **linear velocity only** into Jolt; **Jolt integrates position** until [`readBackKinematics`](../../engine/physics/IPhysicsScene.hpp). Game-side `position` is authoritative only after readback or after [`setBodyCenterAndLinearVelocity`](../../engine/physics/IPhysicsScene.hpp).

3. **Future larger worlds (planning contract)**  
   When scale or multiplayer requires it, **add a game-owned mapping** (e.g. hierarchical / double-precision anchor + **local `float` island**) **without** changing the meaning of [`Vec3`](../../engine/math/Vec3.hpp) in GPU and Jolt paths: **subtract an origin** into the scene, **add it back** after readback. Until that exists, all code may assume **global `float` meters**.

4. **Simulation tiers (intent, not implementation)**  
   **High-fidelity** contact physics applies only in a **bounded region** near active players (order of tens to low hundreds of fully resolved bodies as a **design budget**, not a hardcoded engine limit). Distant or low-priority entities use **cheaper** representation (kinematic, abstract, or non-physical). The whole planet is **not** one full-fidelity Jolt world.

5. **Orbital / cruise motion (MVP direction)**  
   Use **patched conics / sphere-of-influence-style** (or equivalent analytic orbit) for **off-surface** motion; keep a **replaceable** “orbit integrator” seam so N-body or perturbations can be added later without rewriting gameplay around a single gravity model.

6. **Tier handoff invariants (when blending tiers)**  
   - **H1:** After handoff, **linear velocity** in the **agreed inertial frame** matches within a documented **ε** (no arbitrary speed jumps).  
   - **H2:** **Position** is valid for the target tier (e.g. not underground, on agreed trajectory or surface).  
   - **H3:** An entity is **not** full-step simulated in **two tiers** in the same frame (no double integration).  
   **Visual** continuity (interpolation, FX) may exceed **physics** continuity; pops are acceptable only where H1–H3 still hold.

## Consequences

- Positive: one place states **current** behavior and **future** rules for rebasing, streaming, and MP without blocking today’s samples.  
- Positive: tier caps and orbit choice are explicit **design** levers, not accidental.  
- Trade-off: implementing planet scale or MP will require **new types or services above** [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp); this ADR does not prescribe their API.  
- Follow-up: **Phase 0 landed:** [`SimulationIsland`](../../engine/gameplay/SimulationIsland.hpp) holds the double-precision world anchor; Garden applies [`localGameplayToJolt`](../../engine/gameplay/SimulationIsland.hpp) (and static-geometry helpers) at the Jolt boundary while gameplay/layout stay simulation-local. [`garden_simulation_test`](../../tests/garden_simulation_test.cpp) compares a **1e6** anchor run against origin zero to assert stable, matching local motion. **Transport baseline landed:** [`MultiplayerWireFormat.hpp`](../../engine/gameplay/MultiplayerWireFormat.hpp) (anchor + tier handoff wire) + UDP tests ([`udp_game_transport_integration_test`](../../tests/udp_game_transport_integration_test.cpp)). **Next:** session rules for anchor updates over the wire; tier-handoff gameplay path on the live protocol; optional floating rebasing of the anchor when the player moves far in sim-local space ([`multiplayer-physics-world-scale.md`](../architecture/multiplayer-physics-world-scale.md)).
- Follow-up: detailed **multiplayer-first** planning for **tier handoffs as net events**, **fast movers**, and **anchor/world replication** (non-normative) — [`docs/architecture/multiplayer-physics-world-scale.md`](../architecture/multiplayer-physics-world-scale.md).

## Alternatives considered

- **Defer documentation until features ship:** rejected; avoids silent drift and rework.  
- **Mandate double-precision `Vec3` engine-wide:** rejected for now; GPU and Jolt boundaries stay `float`; large-scale fixes target **hierarchy + local island**, not a blanket type swap.
