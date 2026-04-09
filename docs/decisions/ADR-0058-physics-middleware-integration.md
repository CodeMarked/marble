# ADR-0058: Physics middleware integration (Jolt-first)

## Status

Accepted

## Context

[`ADR-0041`](ADR-0041-physics-world-integration-seam.md) introduced `SimplePhysicsWorld` and `PhysicsWorldSettings` as a dependency-free stand-in. The **`engine`** library integrates **[Jolt](https://github.com/jrouwe/JoltPhysics)** behind [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp) ([`createJoltPhysicsScene`](../../engine/physics/IPhysicsScene.hpp), implementation in [`JoltPhysicsScene.cpp`](../../engine/physics/jolt/JoltPhysicsScene.cpp)). The **garden** sample builds a scene from [`GardenLayout`](../../game/garden/GardenSimulation.hpp) AABBs plus two dynamic spheres, reads gravity / sub-steps from [`PhysicsWorldSettings`](../../engine/physics/PhysicsWorld.hpp), and uses optional post-step cylindrical clamping via [`PhysicsStepOptions`](../../engine/physics/MiddlewarePhysicsTypes.hpp). The **`marbles`** sample uses the same [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp) path: static boxes for the tilt-board arena and one dynamic sphere; pickups and fall-reset remain game-side. Tests and code that only need integration semantics still use [`SimplePhysicsWorld`](../../engine/physics/PhysicsIntegration.hpp).

Marble uses **left-handed coordinates, +Y up** ([`ADR-0017`](ADR-0017-three-d-math-conventions-and-vec3.md)). Middleware libraries often default to **right-handed** bases; the adapter layer must apply a single, documented basis mapping when stepping and when authoring shapes (do not scatter sign fixes in gameplay).

## Decision

1. **Primary middleware candidate: [Jolt Physics](https://github.com/jrouwe/JoltPhysics)** (MIT, CMake `FetchContent` with `SOURCE_SUBDIR Build`, MSVC/GCC/Clang). Godot 4 and other engines use it; it fits a PC-first Vulkan codebase.
2. **Alternatives (documented trade-offs)**:
   - **Bullet**: widespread, zlib; heavier CMake and API surface; good if team already ships it.
   - **PhysX**: mature; NVIDIA license and platform matrix; stronger if console targets are fixed early.
3. **Build**: [`cmake/MarbleJolt.cmake`](../../cmake/MarbleJolt.cmake) provides `marble_fetch_jolt_once()`; [`engine/CMakeLists.txt`](../../engine/CMakeLists.txt) invokes it and links **`Jolt` `PRIVATE`** into **`engine`** (v5.5.0). [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) registers [`jolt_physics_smoke_test`](../../tests/jolt_physics_smoke_test.cpp) when target `Jolt` exists (after configuring `engine/`).
4. **Version pin**: v5.5.0 (or newer documented in this ADR when bumped). Use `FetchContent_Declare(..., SOURCE_SUBDIR Build)` as in [JoltPhysicsHelloWorld](https://github.com/jrouwe/JoltPhysicsHelloWorld).
5. **Threading**: Initial integration runs **`PhysicsSystem::Update` on the game thread** ([`ADR-0011`](ADR-0011-baseline-single-thread-and-explicit-parallelism-hints.md)). Jolt’s job system can be mapped to `maxRecommendedWorkerThreads` later; not required for the first link.
6. **Bridge strategy**:
   - **Stage A**: `SimplePhysicsWorld` remains the stand-alone integrator for policy/tests; samples that need collisions use **`IPhysicsScene`** directly (garden, marbles).
   - **Stage B (in progress via [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp))**: Opaque [`PhysicsBodyId`](../../engine/physics/MiddlewarePhysicsTypes.hpp) handles, static box + heightfield + dynamic sphere authoring, [`PhysicsStepOptions`](../../engine/physics/MiddlewarePhysicsTypes.hpp) for gameplay clamps. **Landed:** [`CollisionFilter`](../../engine/physics/CollisionMiddleware.hpp) on static/dynamic descriptors ([`MiddlewarePhysicsTypes.hpp`](../../engine/physics/MiddlewarePhysicsTypes.hpp)), packed into Jolt body user data and enforced in narrow phase via `ContactListener::OnContactValidate` (two-layer broadphase unchanged). **Landed:** [`enableSleeping`](../../engine/physics/PhysicsWorld.hpp) drives Jolt `mAllowSleeping` and runtime toggles via body locks (`jolt_sleeping_test`). **Still open:** capsules/convex hulls, fuller transform sync, optional unified `JoltPhysicsWorld` implementing `IPhysicsWorld`.
7. **Settings**: [`PhysicsWorldSettings`](../../engine/physics/PhysicsWorld.hpp) includes middleware-only fields (`velocitySolverIterations`, `positionSolverIterations`, `enableContinuousCollision`, `enableSleeping`) that [`SimplePhysicsWorld`](../../engine/physics/PhysicsIntegration.hpp) ignores; [`JoltPhysicsScene`](../../engine/physics/jolt/JoltPhysicsScene.cpp) maps solver iterations and CCD-related tunables into Jolt’s [`PhysicsSettings`](https://github.com/jrouwe/JoltPhysics), and maps `enableSleeping` into per-dynamic-body allow-sleep and wake behavior.

## Consequences

- Positive: gameplay depends on **`IPhysicsWorld` / `PhysicsWorldSettings`** for policy and on **`IPhysicsScene`** for middleware-backed rigid bodies; swapping Jolt or adding shapes stays in `engine/physics`.
- Positive: smoke test + garden integration prove fetch + compile + link on CI and dev machines.
- Trade-off: Jolt’s `Build/CMakeLists.txt` sets many options; Marble forces `USE_STATIC_MSVC_RUNTIME_LIBRARY OFF` on MSVC (match `/MDd`) and `ENABLE_ALL_WARNINGS OFF` (and related) before `FetchContent_MakeAvailable`; revisit when upgrading Jolt.
- Follow-up: optional `JoltPhysicsWorld` implementing `IPhysicsWorld` for unified kinematics stepping (marbles now uses `IPhysicsScene` instead); oriented static shapes matching prop yaw/pitch/roll instead of layout world AABBs; basis audit vs ADR-0017 if artifacts appear.

## Alternatives considered

- **Keep Jolt entirely out of `engine`:** superseded; Jolt now links **`PRIVATE`** into **`engine`** while **public** headers remain Jolt-free ([`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp)). Game code uses those engine headers only (no direct Jolt includes in `game/`).
- **Only documentation, no CMake hook:** rejected; optional flag + smoke test reduces integration surprise.
