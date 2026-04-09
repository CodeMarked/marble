# Master Plan

## 1) Snapshot

- Project: Marble game engine runtime and supporting toolchain, evolving toward a **reusable C++ SDK** for **external teams** (not a single title only).
- **Samples** drive subsystem work: **physics and simulation**, **graphics/RHI**, then **multiplayer foundations**; **gameplay logic** stays secondary until those layers are stable enough that APIs do not churn weekly. Phased detail: [`architecture/sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md).
- Current phase: first **Vulkan** path and **`marbles`** demo are on `dev`; architecture study chunks **001–054** remain reflected in policy headers and tests. Near-term engineering gates remain **shader tooling** (see open questions), **RHI boundaries**, **resource manager / ACP**, **input depth** (multi-slot gamepad, disconnect/reconnect, richer menus), and **multiplayer depth** beyond the **landed** UDP + wire + session-envelope baseline: authoritative tick tied to snapshots, reliability/sequencing, **decoded snapshot interpolation** (`SnapshotInterpolator.hpp`), AOI, and benchmark metrics ([`architecture/multiplayer-physics-world-scale.md`](architecture/multiplayer-physics-world-scale.md)). **Still open:** wiring interp into a visual sample, sub-tick render time, prediction, and explicit correction.
- Primary goal: build a stable, testable engine core with clear subsystem boundaries and documented integrator expectations.
- **Implementation depth:** Many study-chunk rows below are **policy and API baselines** (headers, types, tests, ADRs). They are **not** a commitment to immediate **full production stacks** (for example OS audio I/O, a script VM, or **complete** online netcode) everywhere. **Partial exception:** the `engine` library now links **UDP/IPv4** (`UdpSocket`, `UdpGameTransport`) plus **binary wire + session envelope** headers and tests; production-grade prediction, interest management, and NAT traversal are still **not** implied. What gets **linked and shipped in the `engine` library** is a narrower runtime slice; deeper subsystem work follows **sample-driven phases** in [`architecture/sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md). Detail: [`architecture/engine-subsystems.md`](architecture/engine-subsystems.md) (*Implementation depth*).
- **Maintainer priorities (this codebase):** (1) **Physics** — fixed-step integration, [`IPhysicsScene`](../engine/physics/IPhysicsScene.hpp) / Jolt depth, budgets for awake bodies and static scene setup ([`ADR-0058`](decisions/ADR-0058-physics-middleware-integration.md)). (2) **Universe / world generation** — large-scale layout and streaming/procedural pipeline on top of simulation/world space ([`ADR-0059`](decisions/ADR-0059-simulation-space-and-physics-tier-contract.md)), chunk/object contracts ([`WorldDataFormats`](../engine/gameplay/WorldDataFormats.hpp)), and sample paths such as **`garden`** (bake, static colliders, runtime dynamics). (3) **Multiplayer server as early as feasible** — **listen / dedicated headless** simulation, transport, tick + **narrow** replicated state aligned with [`ADR-0054`](decisions/ADR-0054-online-multiplayer-authority-topology-baseline.md), [`ADR-0060`](decisions/ADR-0060-multiplayer-data-plane-and-authority-constraints.md), and [`OnlineMultiplayerFoundation`](../engine/gameplay/OnlineMultiplayerFoundation.hpp); **after** physics + world slice are stable enough not to rewrite netcode weekly. This **front-runs** the generic SDK doc ordering for other integrators in places: accept **API churn** on net/server seams until they settle.

## 2) Current decisions

- Architecture direction is captured and updated as we process each study chunk.
- Subsystem boundaries are explicit before deep feature work.
- Notes and open questions are tracked continuously and promoted into canonical docs when settled.
- High-impact runtime choices are recorded under [`decisions/`](decisions/) (ADR series from 0001 onward; **ADR-0055** covers Vulkan-first RHI + public-repo vs local `docs/` boundary). Newer entries document physics middleware, simulation/world space, multiplayer constraints, and related seams; see `decisions/` for the authoritative list.
- Planning docs in `docs/` target **external C++ integrators** as well as maintainers; **public API surface and versioning** for SDK adoption is **open** until decided ([`notes/OPEN_QUESTIONS.md`](notes/OPEN_QUESTIONS.md)).

## 3) Shipped status

In the table below, **Shipped** for a **chunk / ADR** row means the **contract and baseline** called out in that chunk (header-level or tested seam) is in place. It does **not** mean every related header has a full backend implementation in the same milestone. Rows that name **Vulkan**, **Jolt**, **GLFW**, or concrete `.cpp` targets describe **linked runtime** work; animation, audio math, gameplay seams, and multiplayer *foundation* rows are primarily **baselines** until later phases need more.

| Area | Status |
| --- | --- |
| Book chunking workflow and index | Shipped |
| Documentation baseline and planning structure | Shipped |
| Engine lifecycle baseline (`core::Engine`, init/run/shutdown, frame phases) | Shipped (chunk 001–002) |
| Simulation/render phase interfaces (`ISimulationPhase`, `IRenderPhase`) | Shipped (chunk 002) |
| Fixed-step default simulation (`FixedStepSimulationPhase`) | Shipped (chunk 003) |
| Runtime assets root resolution + CMake staging of `assets/` beside `marbles` | Shipped (chunk 004) |
| Preset-driven preflight workflow + diagnostics interval runtime knob | Shipped (chunk 005) |
| Toolchain compile policy (`marble_compile_options`, `MARBLE_DEBUG`, MSVC `/W4`) | Shipped (chunk 006) |
| Assertions / unreachable policy (`MARBLE_ASSERT`, `MARBLE_UNREACHABLE`) | Shipped (chunk 007) |
| Alignment / cache-line helpers (`MemoryLayout.hpp`) | Shipped (chunk 008) |
| OS hardware queries (`platform/hardware/System.hpp`: page size, logical processor count) | Shipped (chunk 009) |
| Cache-line padding type (`MemoryArchitecture.hpp`: `CacheLinePad`) | Shipped (chunk 010) |
| Parallelism policy (`Parallelism.hpp`: no engine worker threads; `maxRecommendedWorkerThreads`) | Shipped (chunk 011) |
| OS boundary / process id (`platform/os/ProcessId.hpp`) | Shipped (chunk 012) |
| Synchronization vocabulary (`SyncPrimitives.hpp`: mutex, locks, condition variable) | Shipped (chunk 013) |
| Lock ordering (`LockOrdering.hpp`: `LockLevel`, `mayAcquireAfter`) | Shipped (chunk 014) |
| Atomics vocabulary (`Atomics.hpp`) | Shipped (chunk 015) |
| SIMD buffer alignment (`Simd.hpp`) | Shipped (chunk 016) |
| 3D vector math (`math/Vec3.hpp`, conventions ADR-0017) | Shipped (chunk 017) |
| 4×4 transforms (`math/Mat4.hpp`, column-major; ADR-0018) | Shipped (chunk 018) |
| Geometry primitives + RNG policy (`math/Geometry.hpp`, `math/Random.hpp`; ADR-0019) | Shipped (chunk 019) |
| Engine support ordering + stack allocator baseline (`core/StackAllocator.hpp`; ADR-0020) | Shipped (chunk 020) |
| Container baseline (`core/ClosedHashTable.hpp`; ADR-0021) | Shipped (chunk 021) |
| String IDs + config store baseline (`core/StringId.hpp`, `core/ConfigStore.hpp`; ADR-0022) | Shipped (chunk 022) |
| File system path + synchronous I/O baseline (`platform/filesystem/*`; ADR-0023) | Shipped (chunk 023) |
| Runtime resource registry + lifetime baseline (`core/ResourceManager.hpp`; ADR-0024) | Shipped (chunk 024) |
| Time measurement + frame-delta policy (`core/Time.hpp`; ADR-0025) | Shipped (chunk 025) |
| Multiprocessor loop seam baseline (`core/JobSystem.hpp`; ADR-0026) | Shipped (chunk 026) |
| HID input processing baseline (`input/Hid.hpp`; ADR-0027) | Shipped (chunk 027) |
| HID chord/gesture baseline (`input/Hid.hpp`; ADR-0028) | Shipped (chunk 028) |
| HID abstraction/remapping baseline (`input/HidMapping.hpp`; ADR-0029) | Shipped (chunk 029) |
| HID context ownership + action-disable policy baseline (`input/HidMapping.hpp`; ADR-0030) | Shipped (chunk 030) |
| Logging/tracing baseline (`core/Log.hpp`, `core/Log.cpp`; ADR-0031) | Shipped (chunk 031) |
| Skeleton parent ordering + local→global pose FK (`animation/SkeletonPose.hpp`; ADR-0032) | Shipped (chunk 032) |
| Animation clips + skinning matrix palette (`animation/AnimationClip.hpp`, `animation/Skinning.hpp`; ADR-0033) | Shipped (chunk 033) |
| Animation blending + local post-process (`animation/AnimationBlend.hpp`; ADR-0034) | Shipped (chunk 034) |
| Animation compression + pipeline order doc (`animation/AnimationCompression.hpp`; ADR-0035) | Shipped (chunk 035) |
| Action state machine (`animation/ActionStateMachine.hpp`; ADR-0036) | Shipped (chunk 036) |
| Animation constraints (`animation/Constraints.hpp`; ADR-0037) | Shipped (chunk 037) |
| Collision middleware primitives + layer filters (`physics/CollisionMiddleware.hpp`; ADR-0038) | Shipped (chunk 038) |
| Collision detection raycasts + naive AABB broad-phase pairs (`physics/CollisionDetection.hpp`; ADR-0039) | Shipped (chunk 039) |
| Rigid body dynamics integration (`physics/RigidBodyDynamics.hpp`; ADR-0040) | Shipped (chunk 040) |
| Physics world integration seam (`physics/PhysicsIntegration.hpp`; ADR-0041) | Shipped (chunk 041) |
| Physics of sound baseline (`audio/PhysicsOfSound.hpp`; ADR-0042) | Shipped (chunk 042) |
| Mathematics of sound baseline (`audio/MathematicsOfSound.hpp`; ADR-0043) | Shipped (chunk 043) |
| Sound technology PCM layout (`audio/SoundTechnology.hpp`; ADR-0044) | Shipped (chunk 044) |
| Spatial audio 3D baseline (`audio/AudioSpatial3D.hpp`; ADR-0045) | Shipped (chunk 045) |
| Audio engine architecture voice graph (`audio/AudioEngineArchitecture.hpp`; ADR-0046) | Shipped (chunk 046) |
| Game-specific audio policy baseline (`audio/GameSpecificAudio.hpp`; ADR-0047) | Shipped (chunk 047) |
| Gameplay runtime and world editor foundation (`gameplay/GameplayFoundation.hpp`; ADR-0048) | Shipped (chunk 048) |
| Runtime object model baseline (`gameplay/RuntimeObjectModel.hpp`; ADR-0049) | Shipped (chunk 049) |
| World chunk data/object refs/query baseline (`gameplay/WorldDataFormats.hpp`; ADR-0050) | Shipped (chunk 050) |
| Real-time object updates/batch seam (`gameplay/RealtimeObjectUpdates.hpp`; ADR-0051) | Shipped (chunk 051) |
| Events and message-passing baseline (`gameplay/EventsAndMessaging.hpp`; ADR-0052) | Shipped (chunk 052) |
| Scripting bindings + high-level game flow (`gameplay/ScriptingAndGameFlow.hpp`; ADR-0053) | Shipped (chunk 053) |
| Online multiplayer authority/topology baseline (`gameplay/OnlineMultiplayerFoundation.hpp`; ADR-0054) | Shipped (chunk 054) |
| Real-time transport + wire format + session envelope (`IGameTransport`, `UdpGameTransport`, `UdpSocket`, `MultiplayerWireFormat.hpp`, `MultiplayerSessionEnvelope.hpp`; ADR-0060 seam) | Shipped (narrow baseline; not full netcode — see [`architecture/multiplayer-physics-world-scale.md`](architecture/multiplayer-physics-world-scale.md)) |
| Phase 3 sample `mp_foundation` (UDP + `AuthoritativeSession` / `ClientSession`: raw peer discovery, reliable HelloAck, framed kinematics snapshots) | Shipped |
| Reliable control channel on UDP (`ReliableChannel.hpp`; sequence + ack + retransmit; protocol version 2; `MultiplayerSessionEnvelope.hpp` Ack/Disconnect message types + reliable header) | Shipped |
| Authoritative session + client session (`AuthoritativeSession.hpp`, `ClientSession.hpp`; fixed tick driving roster + per-peer reliable channels + snapshot emission; client snapshot ring buffer; loopback + integration tests) | Shipped |
| Client snapshot interpolation helper (`SnapshotInterpolator.hpp`; server-tick-space lerp + velocity extrapolation clamp; `snapshot_interpolator_test`) | Shipped (narrow; decode ring → push → interpolate — not wired into `marbles` render loop) |
| First Vulkan RHI + swapchain presentation path (`engine/render/vulkan/VulkanRhi`; ADR-0055) | Shipped |
| `marbles` sample: tilt board, pickups, `IRenderPhase` + GLSL→SPIR-V (`glslc`), staged shaders/assets | Shipped |
| Platform keyboard → abstract controls + remap (`input/PlatformKeyboardBridge`; marbles uses `InputRemapTable` / `actionScalar`) | Shipped |
| Platform gamepad merge (GLFW gamepad layout → `AbstractControl`; `mergeFirstGamepadIntoAbstractControls`) | Shipped |
| Expanded `platform::Window` key surface + extra keyboard → abstract mappings (`PlatformKeyboardBridge`) | Shipped |
| `ActionPolicy` batched context transitions (`ActionContextEntry`, `resetActionGates`, `applyActionContext`; marbles gameplay vs victory) | Shipped |
| Render submission seam (`MeshDrawInstance`, `IRenderBackend`, `VulkanRhi` implements; ADR-0056) | Shipped |
| Shader compilation baseline ADR (`glslc` offline, SPIR-V staged; ADR-0057) | Shipped |
| `BinaryResourceManager` search-root helper + `ResourceKind`; marbles primes registry with `README.txt` when assets root resolves; SPIR-V mesh shaders load via registry + `VulkanRhi::initFromSpirvBytes` when assets root resolves (fallback: `exe/shaders`) | Shipped |
| Automated tests: smoke, fixed-step, asset root, policy, memory layout, hardware system, memory architecture, parallelism policy, OS process, synchronization primitives, lock ordering, lock-free primitives, SIMD policy, vec3 math, mat4 math, geometry math, random policy, stack allocator, closed hash table, string id, config store, filesystem path/io, resource manager, time policy, job system baseline, HID input policy, HID gesture policy, HID mapping policy, HID action policy, HID action context (`applyActionContext`), platform keyboard bridge, platform gamepad bridge, log policy, animation pose, animation clip/skinning, animation blend, animation compression, animation action state machine, animation constraints, physics middleware, collision detection, rigid body dynamics, physics world integration, garden simulation (Jolt), jolt sleeping, jolt collision filter, physics of sound, mathematics of sound, sound technology, spatial audio 3D, audio engine architecture, game-specific audio, gameplay foundation, runtime object model, world data formats, realtime object updates, events and messaging, scripting game flow, online multiplayer foundation, multiplayer wire format, multiplayer session envelope, loopback game transport, UDP game transport integration, reliable channel, reliable envelope, authoritative session, snapshot interpolator, engine phase reset, IRenderBackend seam | Shipped |
| First concrete render phase (Vulkan swapchain / frame output) | Shipped (see Vulkan RHI row); further render features backlog below |
| Resource manager / full ACP (packs, importers, typed mesh/texture loaders) | In progress (runtime registry + search root + `ResourceKind` baseline) |
| Subsystem map + phased “fleshing” policy (baseline vs SDK-critical runtime) | Shipped ([`architecture/engine-subsystems.md`](architecture/engine-subsystems.md)) |

## 4) Roadmap

Backlog items are grouped by **theme**, not by P0/P1. **Order of attack:** clear **planning gates** first; then **physics and universe** and **multiplayer server** (see §1 *Maintainer priorities*); then **rendering** and **assets** as needed; run **input**, **docs/ADR cleanup**, **platform validation**, and **hardening** in parallel when they do not block those lanes.

### Planning gates

- **Met (2026-04-03):** [`notes/OPEN_QUESTIONS.md`](notes/OPEN_QUESTIONS.md) — **Owner audit** paragraph, **Milestone role** column (blocking vs track vs closed), **Current roadmap (blocking vs track)** (chunk-gated list + baseline ADR allowance), and **Recorded deferrals**; aligned with [`guardrails/architecture-guardrails.md`](guardrails/architecture-guardrails.md). *Original done-when:* owner column audited (including closed rows); roadmap names what blocks shader/RHI/ACP versus chunk-gated follow-through.

### Physics and universe / world generation

- Fixed-step + [`IPhysicsScene`](../engine/physics/IPhysicsScene.hpp) / Jolt ([`ADR-0058`](decisions/ADR-0058-physics-middleware-integration.md)), simulation space ([`ADR-0059`](decisions/ADR-0059-simulation-space-and-physics-tier-contract.md)), large-yard static bake + runtime dynamics (`garden` trajectory).
- Chunk layout, object refs, queries, and generation hooks toward [`WorldDataFormats`](../engine/gameplay/WorldDataFormats.hpp); keep `marbles` and tests as regression anchors.
- **Multiplayer-first planning (Dream-oriented):** dense environments, fast movers, tier handoffs, and replication budgets are captured in [`architecture/multiplayer-physics-world-scale.md`](architecture/multiplayer-physics-world-scale.md) (non-normative; cross-links ADR-0054/0059/0060).

### Multiplayer server

- **Landed (narrow):** UDP/IPv4 datagram path (`UdpSocket`, `UdpGameTransport`), endian-safe kinematics/tier-handoff wire (`MultiplayerWireFormat.hpp`), session framing (`MultiplayerSessionEnvelope.hpp`), loopback + localhost UDP tests, and **`mp_foundation`** sample (`AuthoritativeSession` / `ClientSession`: discovery + reliable handshake + periodic snapshots). See [`architecture/multiplayer-and-platform-baseline.md`](architecture/multiplayer-and-platform-baseline.md) and [`architecture/multiplayer-physics-world-scale.md`](architecture/multiplayer-physics-world-scale.md).
- **Early path (landed):** dedicated / listen authoritative session with **fixed tick** driving **small** replicated state, wired to the same protocol ([`ADR-0054`](decisions/ADR-0054-online-multiplayer-authority-topology-baseline.md), [`ADR-0060`](decisions/ADR-0060-multiplayer-data-plane-and-authority-constraints.md), [`OnlineMultiplayerFoundation`](../engine/gameplay/OnlineMultiplayerFoundation.hpp), [`AuthoritativeSession`](../engine/gameplay/AuthoritativeSession.hpp), [`ClientSession`](../engine/gameplay/ClientSession.hpp)). **Headless** standalone binary not yet shipped; the session class is tick-driven and usable from any loop.
- **Prioritized follow-ups (engineering + impressive stats):** (1) ~~**Reliable control messages** on UDP~~ — **landed** (`ReliableChannel.hpp`, protocol version 2, sequence + ack + retransmit for control; bulk pose stays unreliable). (2) **Client snapshot timeline** — ring buffer **landed** (`ClientSession.hpp`); **interpolation baseline landed** (`SnapshotInterpolator.hpp`, `snapshot_interpolator_test` — server-tick-space lerp + extrapolation clamp; caller decodes ring entries). **Still open:** sub-tick / render-clock mapping, light prediction, explicit correction/reconciliation, wiring into a visual sample. (3) **Interest management** — velocity-aware AOI, update-rate buckets, bytes/tick budgets. (4) **Benchmark harness** — synthetic clients, optional loss/jitter/reorder; (5) **Exported metrics** — p50/p95 RTT, bytes/s per peer, snapshot age, correction counts (CI- or release-note friendly). Full platform services (matchmaking, accounts) and NAT traversal remain **explicitly later** unless an open question pulls them forward.
- **Optional / small (not required for Phase 3 narrative):** **Handshake retry** is **landed** on the client via `ClientSession` (`mp_foundation` uses it); optional follow-up is **backoff tuning** or a hard cap policy shared across samples. Continue **deterministic negative-path tests** (truncated buffers, bad magic, version mismatch); treat **coverage** as the default QA story.
- **Defer (hardening sprint):** **Binary protocol fuzzing** (e.g. libFuzzer/AFL on parse paths) — valuable after framing + reliable control stabilize; redundant with fuzz wording on resumes until a fuzz target exists. See [`architecture/multiplayer-physics-world-scale.md`](architecture/multiplayer-physics-world-scale.md#optional-and-deferred).
- **Design follow-up:** extend authority/topology docs (local / self-host / central) as the server slice lands; align with message-passing context (chunk **052**) where relevant. For physics/world scale, velocity-aware interest, tier handoff packets, and replication taxonomy, see [`architecture/multiplayer-physics-world-scale.md`](architecture/multiplayer-physics-world-scale.md).

### Rendering (RHI and shaders)

- **RHI:** grow from ADR-0056 (`IRenderBackend`, submission seam) toward passes/materials/instancing when samples or debug need them; a **second backend** forces interface review.
- **Shaders:** baseline ADR-0057 (`glslc`); revisit **libshaderc** / checked-in SPIR-V when open questions, ACP, or hot reload demand it.

### Assets and ACP

- Advance from runtime registry + search root + `ResourceKind` toward **packs**, **importers**, and **typed mesh/texture loaders** (§3 shipped table *in progress*); prioritize when universe/content volume requires it.

### Input

- Multi-slot gamepad routing; disconnect/reconnect and battery/diagnostic surfacing; optional chord grace-window tests (ADR-0027/0028 traceability); menu/UI context presets beyond `marbles`.

### Documentation and ADR hygiene

- Expand subsystem notes for rendering, simulation workload, assets, gameplay boundaries ([`architecture/engine-subsystems.md`](architecture/engine-subsystems.md) — add pass/material and ACP milestones as they land).
- Close stale follow-up bullets inside accepted ADRs (e.g. ADR-0001 vs ADR-0002/0003).

### Platform validation

- **Linux / macOS:** run **`ctest`** (or CI) on POSIX hosts to validate `platform::executableDirectory()` and assets paths (ADR-0004 follow-up); Windows-only validation is insufficient.

### Performance, guardrails, and traceability

- Profiling guidance and checkpoints (CI or documented thresholds).
- Operational guardrails as complexity grows.
- Keep [`runbooks/adr-test-traceability.md`](runbooks/adr-test-traceability.md) current as ADRs and tests evolve.

### Deferred unless open questions pull them forward

- **OS audio output** / full device backend; **script VM**.

### Sample and SDK sequence

Canonical phased order (dependencies, parallel work, cross-platform, doc distribution): [`architecture/sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md). **Next concrete sample priorities:** (1) **simulation + physics** vertical slice on fixed-step, (2) **render/RHI depth** in parallel as samples require, (3) **narrow multiplayer foundation** sample (authority + tick + small replicated state). Gameplay-heavy samples follow later phases in that doc.

**Maintainer alignment:** This repo **emphasizes** (1) and **universe/world generation** work alongside it, and **accelerates** (3) toward a **real server process + transport** as early as the sim/world contract allows—see §1 *Maintainer priorities* and §4 *Roadmap*. External SDK readers should still treat [`sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md) as the generic phase story; Marble may front-run **server** work relative to that doc where dependencies allow.

The **garden** sample uses **human / football scale** on an acre-class yard: terrain and props are **mostly static colliders** after a **phased bake** (log-only high-substep AABB settle, then freeze). **Runtime marbles** use **Jolt** through [`IPhysicsScene`](../engine/physics/IPhysicsScene.hpp) in **engine**; broader rigid-body gameplay (many dynamic props, vehicles) remains follow-up work on the same middleware path.

**Physics middleware:** [`IPhysicsWorld`](../engine/physics/PhysicsWorld.hpp) + [`SimplePhysicsWorld`](../engine/physics/PhysicsIntegration.hpp) for tests and samples that only need integration; **engine** links **[Jolt](https://github.com/jrouwe/JoltPhysics)** `PRIVATE` behind [`IPhysicsScene`](../engine/physics/IPhysicsScene.hpp) ([`ADR-0058`](decisions/ADR-0058-physics-middleware-integration.md)). **`marbles`** uses **`IPhysicsScene` / Jolt** (tilt-board static boxes + dynamic sphere); **garden** uses Jolt for runtime marbles and `SimplePhysicsWorld` for optional tooling paths as documented in code.

**Future gameplay scale (targets, not commitments):** A plausible long-term direction is **many destructible, movable props** and **multiplayer** mayhem (lots of simultaneous dynamics). A maintainer **north-star** stress case is **Dream**-style play: **high-speed** vehicles through **dense** spaces with **large FX** and **tier transitions** (surface contact to orbit), which hits **replication** and **velocity-based interest** before raw CPU/GPU limits. Plan for **explicit budgets** (awake bodies, replicated entities), **sleep/LOD** for distant props, and **tiered destruction** (what the server authors vs client-local debris/fx). See [`architecture/sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md) for phased sample work and [`architecture/multiplayer-physics-world-scale.md`](architecture/multiplayer-physics-world-scale.md) for consolidated multiplayer-first physics/world design notes.

## 5) Open questions

Canonical table: [`notes/OPEN_QUESTIONS.md`](notes/OPEN_QUESTIONS.md). Each row uses a **book-chunk target gate** (not calendar dates) unless a release milestone forces an early decision.

High-level themes still open:

- Engine core vs gameplay runtime boundary.
- Concurrency default for first production-ready milestone.
- Asset pipeline: intermediate formats and conditioning stages before runtime packs.
- Fixed-step stall policy (drop vs catch-up) under load.
- Shader compilation and CI/dev alignment (CLI vs library vs offline SPIR-V).
- Public documentation strategy given local-only `docs/` (see OPEN_QUESTIONS).
- Public C++ API surface and **versioning / compatibility** for external SDK consumers (see OPEN_QUESTIONS).

## 6) Maintenance

- Update this file whenever priorities, decisions, or architecture direction changes; when **SDK/sample phases** move, update [`architecture/sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md) in the same pass when milestones shift.
- Keep deep technical detail in `architecture/*`, `guardrails/*`, and `decisions/*`.
- When adjusting **which subsystems are baselines vs actively implemented**, update [`architecture/engine-subsystems.md`](architecture/engine-subsystems.md) and, if phase gates move, [`architecture/sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md) in the same pass.
