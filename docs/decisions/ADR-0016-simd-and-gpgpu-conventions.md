# ADR-0016: SIMD alignment and GPGPU scope (no intrinsics or compute yet)

## Status

Accepted

## Context

Chapters 4.10–4.11 introduce **SIMD/vector** execution and **GPGPU** (wide parallel hardware, historically graphics-focused, now general compute). Marble already links **Vulkan** for future rendering work, but has **no** SIMD intrinsics layer, **no** GPU compute pipelines, and **no** math library yet (book **Chapter 5** / chunk **017** is the dedicated math slice).

## Decision

1. Publish SIMD **buffer alignment** helpers in [`engine/core/Simd.hpp`](../../engine/core/Simd.hpp): **16** / **32** / **64** bytes for 128- / 256- / 512-bit class widths, plus [`simdBufferAlignBytes`](../../engine/core/Simd.hpp). Pair with [`MemoryLayout::alignUp`](../../engine/core/MemoryLayout.hpp) for **sizes**, not just alignment.
2. Treat **compiler intrinsics** (`immintrin.h`, NEON intrinsics, etc.) as **opt-in per translation unit** when a hot loop is proven by profiling; do not blanket-include platform SIMD headers from common engine headers.
3. **GPGPU / compute**: Vulkan remains the intended path for **GPU work**, but **compute dispatch**, **pipelines**, and **shader modules** are **out of scope** until the first concrete `IRenderPhase` or compute-backed subsystem lands; this ADR only records **alignment and layering** expectations.
4. **Relationship to ADR-0008**: cache-line size (`kCacheLineBytes`) addresses **false sharing**; SIMD alignment addresses **load/store** constraints—they are **orthogonal** (a buffer may need `max(cacheLine, simd)` alignment for a given use).

## Consequences

- Positive: one place for “how aligned should this scratch buffer be for SIMD?” before a math module exists.
- Positive: avoids pretending a portable `float4` type exists before chunk **017**.
- Trade-off: no runtime feature detection (AVX/AVX-512); callers that need it must query CPU caps later or use compiler-generated vectorized code.

## Alternatives considered

- **Vendor math library now (DirectXMath, GLM, etc.):** deferred to chunk **017** and following math chunks.
- **Implement SIMD wrapper types in `core`:** rejected as premature without math types or workloads.
