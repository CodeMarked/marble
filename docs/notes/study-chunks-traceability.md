# Book study chunks → ADR traceability

This file **replaces** the per-chunk working notes (`CHUNK_*_ANALYSIS.md`, `CHUNK_002_INTAKE_CHECKLIST.md`). Those notes duplicated what is already recorded here:

- **Decisions:** [`docs/decisions/`](../decisions/) (`ADR-0001`–`ADR-0054` for chunks 001–054).
- **Shipped status table:** [`MASTER_PLAN.md`](../MASTER_PLAN.md) §3.
- **Code ↔ ADR mapping:** [`runbooks/adr-test-traceability.md`](../runbooks/adr-test-traceability.md).

Per-chunk analyses added book section mapping, “what landed in code,” and verification commands; the **ADRs**, **tests**, and **traceability runbook** are the durable sources for that.

## Book PDF index

Chunk boundaries and source PDF filenames: [`book/chunks/index.md`](../book/chunks/index.md).

## Chunk → reading slice → ADR

| Chunk | Book reading slice | ADR |
| --- | --- | --- |
| 001 | Front Matter | [ADR-0001](../decisions/ADR-0001-deterministic-single-thread-runtime-loop.md) |
| 002 | 1 Introduction -> 1.4 Engine Differences across Genres | [ADR-0002](../decisions/ADR-0002-runtime-phase-interfaces.md) |
| 003 | 1.5 Game Engine Survey -> 1.6 Runtime Engine Architecture | [ADR-0003](../decisions/ADR-0003-fixed-step-simulation-phase.md) |
| 004 | 1.7 Tools and the Asset Pipeline | [ADR-0004](../decisions/ADR-0004-runtime-assets-root.md) |
| 005 | 2 Tools of the Trade | [ADR-0005](../decisions/ADR-0005-preset-preflight-and-diagnostics-knob.md) |
| 006 | 2.2 Compilers, Linkers and IDEs -> 2.5 Other Tools | [ADR-0006](../decisions/ADR-0006-toolchain-compile-policy.md) |
| 007 | 3 Fundamentals of Software Engineering for Games -> 3.2 Catching and Handling Errors | [ADR-0007](../decisions/ADR-0007-software-engineering-invariants.md) |
| 008 | 3.3 Data, Code and Memory Layout | [ADR-0008](../decisions/ADR-0008-data-and-memory-layout-conventions.md) |
| 009 | 3.4 Computer Hardware Fundamentals | [ADR-0009](../decisions/ADR-0009-hardware-query-surface.md) |
| 010 | 3.5 Memory Architectures | [ADR-0010](../decisions/ADR-0010-memory-architecture-conventions.md) |
| 011 | 4 Parallelism and Concurrent Programming -> 4.3 Explicit Parallelism | [ADR-0011](../decisions/ADR-0011-baseline-single-thread-and-explicit-parallelism-hints.md) |
| 012 | 4.4 Operating System Fundamentals | [ADR-0012](../decisions/ADR-0012-os-boundary-and-process-identity.md) |
| 013 | 4.5 Introduction to Concurrent Programming -> 4.6 Thread Synchronization Primitives | [ADR-0013](../decisions/ADR-0013-thread-synchronization-vocabulary.md) |
| 014 | 4.7 Problems with Lock-Based Concurrency -> 4.8 Some Rules of Thumb for Concurrency | [ADR-0014](../decisions/ADR-0014-lock-ordering-and-deadlock-avoidance.md) |
| 015 | 4.9 Lock-Free Concurrency | [ADR-0015](../decisions/ADR-0015-lock-free-and-atomic-memory-order.md) |
| 016 | 4.10 SIMD/Vector Processing -> 4.11 Introduction to GPGPU Programming | [ADR-0016](../decisions/ADR-0016-simd-and-gpgpu-conventions.md) |
| 017 | 5 3D Math for Games -> 5.2 Points and Vectors | [ADR-0017](../decisions/ADR-0017-three-d-math-conventions-and-vec3.md) |
| 018 | 5.3 Matrices -> 5.4 Quaternions | [ADR-0018](../decisions/ADR-0018-mat4-and-transforms.md) |
| 019 | 5.5 Comparison of Rotational Representations -> 5.7 Random Number Generation | [ADR-0019](../decisions/ADR-0019-geometry-primitives-and-rng-policy.md) |
| 020 | 6 Engine Support Systems -> 6.2 Memory Management | [ADR-0020](../decisions/ADR-0020-engine-support-startup-order-and-stack-allocator.md) |
| 021 | 6.3 Containers | [ADR-0021](../decisions/ADR-0021-container-baseline-closed-hash-table.md) |
| 022 | 6.4 Strings -> 6.5 Engine Configuration | [ADR-0022](../decisions/ADR-0022-string-ids-and-config-store.md) |
| 023 | 7 Resources and the File System -> 7.1 File System | [ADR-0023](../decisions/ADR-0023-filesystem-path-and-sync-io-baseline.md) |
| 024 | 7.2 The Resource Manager | [ADR-0024](../decisions/ADR-0024-runtime-resource-registry-and-lifetime-baseline.md) |
| 025 | 8 The Game Loop and Real-Time Simulation -> 8.5 Measuring and Dealing with Time | [ADR-0025](../decisions/ADR-0025-time-measurement-and-delta-policy.md) |
| 026 | 8.6 Multiprocessor Game Loops | [ADR-0026](../decisions/ADR-0026-multiprocessor-loop-seams-and-inline-job-baseline.md) |
| 027 | 9 Human Interface Devices -> 9.5 Game Engine HID Systems | [ADR-0027](../decisions/ADR-0027-hid-input-processing-baseline.md) |
| 028 | 9.6 Human Interface Devices in Practice -> 10.8 In-Game Profiling | [ADR-0028](../decisions/ADR-0028-hid-chords-and-gesture-detection-baseline.md) |
| 029 | 10.9 In-Game Memory Stats and Leak Detection -> 11.1 Foundations of Depth-Buffered Triangle Rasterization | [ADR-0029](../decisions/ADR-0029-hid-multi-device-abstraction-and-remapping-baseline.md) |
| 030 | 11.2 The Rendering Pipeline | [ADR-0030](../decisions/ADR-0030-hid-context-ownership-and-input-disable-policy.md) |
| 031 | 11.3 Advanced Lighting and Global Illumination -> 11.5 Further Reading | [ADR-0031](../decisions/ADR-0031-logging-and-tracing-baseline.md) |
| 032 | 12 Animation Systems -> 12.3 Poses | [ADR-0032](../decisions/ADR-0032-skeleton-local-global-pose-baseline.md) |
| 033 | 12.4 Clips -> 12.5 Skinning and Matrix Palette Generation | [ADR-0033](../decisions/ADR-0033-animation-clips-and-skinning-palette-baseline.md) |
| 034 | 12.6 Animation Blending -> 12.7 Post-Processing | [ADR-0034](../decisions/ADR-0034-animation-blending-and-post-process-baseline.md) |
| 035 | 12.8 Compression Techniques -> 12.9 The Animation Pipeline | [ADR-0035](../decisions/ADR-0035-animation-compression-and-pipeline-order.md) |
| 036 | 12.10 Action State Machines | [ADR-0036](../decisions/ADR-0036-action-state-machine-baseline.md) |
| 037 | 12.11 Constraints | [ADR-0037](../decisions/ADR-0037-animation-constraints-baseline.md) |
| 038 | 13 Collision and Rigid Body Dynamics -> 13.2 Collision/Physics Middleware | [ADR-0038](../decisions/ADR-0038-collision-middleware-primitives-and-filters.md) |
| 039 | 13.3 The Collision Detection System | [ADR-0039](../decisions/ADR-0039-collision-detection-raycast-and-naive-broadphase.md) |
| 040 | 13.4 Rigid Body Dynamics | [ADR-0040](../decisions/ADR-0040-rigid-body-dynamics-integration-baseline.md) |
| 041 | 13.5 Integrating a Physics Engine into Your Game -> 13.6 Advanced Physics Features | [ADR-0041](../decisions/ADR-0041-physics-world-integration-seam.md) |
| 042 | 14 Audio -> 14.1 The Physics of Sound | [ADR-0042](../decisions/ADR-0042-physics-of-sound-baseline.md) |
| 043 | 14.2 The Mathematics of Sound | [ADR-0043](../decisions/ADR-0043-mathematics-of-sound-baseline.md) |
| 044 | 14.3 The Technology of Sound | [ADR-0044](../decisions/ADR-0044-sound-technology-pcm-layout-baseline.md) |
| 045 | 14.4 Rendering Audio in 3D | [ADR-0045](../decisions/ADR-0045-spatial-audio-3d-baseline.md) |
| 046 | 14.5 Audio Engine Architecture | [ADR-0046](../decisions/ADR-0046-audio-engine-architecture-voice-graph-baseline.md) |
| 047 | 14.6 Game-Specific Audio Features | [ADR-0047](../decisions/ADR-0047-game-specific-audio-features-policy-baseline.md) |
| 048 | 15 Introduction to Gameplay Systems -> 15.4 The Game World Editor | [ADR-0048](../decisions/ADR-0048-gameplay-runtime-and-world-editor-foundation.md) |
| 049 | 16 Runtime Gameplay Foundation Systems -> 16.2 Runtime Object Model Architectures | [ADR-0049](../decisions/ADR-0049-runtime-object-model-generational-store-baseline.md) |
| 050 | 16.3 World Chunk Data Formats -> 16.5 Object References and World Queries | [ADR-0050](../decisions/ADR-0050-world-chunk-data-object-references-and-queries-baseline.md) |
| 051 | 16.6 Updating Game Objects in Real Time -> 16.7 Applying Concurrency to Game Object Updates | [ADR-0051](../decisions/ADR-0051-realtime-object-updates-and-batch-concurrency-seam.md) |
| 052 | 16.8 Events and Message-Passing | [ADR-0052](../decisions/ADR-0052-events-and-message-passing-baseline.md) |
| 053 | 16.9 Scripting -> 16.10 High-Level Game Flow | [ADR-0053](../decisions/ADR-0053-scripting-bindings-and-high-level-game-flow-baseline.md) |
| 054 | 17 You Mean There's More? -> 17.2 Gameplay Systems | [ADR-0054](../decisions/ADR-0054-online-multiplayer-authority-topology-baseline.md) |

## Intake checklist (template for future chunks)

Use when starting a **new** book slice after 054 (or a non-chunk ADR):

1. **Inputs:** chunk PDF (see [`book/chunks/index.md`](../book/chunks/index.md)), current [`system-overview.md`](../architecture/system-overview.md) / [`engine-subsystems.md`](../architecture/engine-subsystems.md), prior ADR if sequential, [`OPEN_QUESTIONS.md`](OPEN_QUESTIONS.md).
2. **Entry:** previous chunk merged and building; verification exists for prior slice (tests or runbook note).
3. **Extraction:** 3–7 architecture implications; each → adopt now / defer with rationale / reject with rationale; each adopted → concrete code or doc change.
4. **Guardrails:** preserve fixed-step and dependency direction unless an ADR changes it; prefer interfaces over large subsystems in one step.
5. **Exit:** ADR drafted or updated, tests or traceability row added, open questions updated, [`MASTER_PLAN.md`](../MASTER_PLAN.md) §3 if shipped table changes.
