# ADR-0033: Animation clips and skinning matrix palette baseline (Chapter 12 §12.4–§12.5)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 033** in **§12.4 Clips** through **§12.5 Skinning and Matrix Palette Generation**. With local→global poses (ADR-0032), the next seams are **time-sampled clip data** driving local poses and **palette matrices** for skinned meshes (`global * inverseBind` per joint).

## Decision

1. Add `[animation/AnimationClip.hpp](../../engine/animation/AnimationClip.hpp)`:
   - keyframes stored **key-major**: `keyframes[keyIndex * jointCount + jointIndex]`,
   - `sampleClipLocalPoses` with uniform key spacing over `durationSeconds`, time clamped to `[0, duration]`,
   - `lerpMat4Elements` for **numeric** matrix lerp (appropriate for translation-heavy test content; **not** rigid rotation interpolation — quaternion slerp deferred).
2. Add `[animation/Skinning.hpp](../../engine/animation/Skinning.hpp)` with `computeMatrixPalette` implementing `palette[i] = globalPose[i] * inverseBindPose[i]` (standard skinning palette before vertex weights).
3. Verify with `animation_clip_skinning_test` (clip sampling + palette with FK from ADR-0032).

## Consequences

- Positive: clear data layout for future clip assets and GPU uniform packing.
- Positive: skinning hook is isolated from rendering backends.
- Trade-off: no variable time keys, compression, or blend trees yet (chunk 034+).
- Follow-up: quaternion channels, slerp, additive layers, and vertex weight buffers / mesh binding.

## Alternatives considered

- **Separate T/R/S channels per joint:** deferred to keep the chunk surface small; current layout matches a flattened pose key table.
- **Dual quaternion skinning:** out of scope for the baseline palette API.
