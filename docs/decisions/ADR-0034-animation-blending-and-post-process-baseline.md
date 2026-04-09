# ADR-0034: Animation blending and local post-process baseline (Chapter 12 §12.6–§12.7)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 034** in **§12.6 Animation Blending** through **§12.7 Post-Processing**. With clips (ADR-0033) producing local poses, runtime animation needs **cross-fades between poses** and **procedural adjustments** after clip evaluation and before forward kinematics.

## Decision

1. Add `[animation/AnimationBlend.hpp](../../engine/animation/AnimationBlend.hpp)`:
   - `blendWeightClamp` — clamp layer/cross-fade weight to `[0, 1]`,
   - `blendLocalPoses` — per-joint blend via `lerpMat4Elements` (same matrix-lerp limits as ADR-0033),
   - `addLocalJointTranslation` — additive translation on one joint’s local affine matrix (typical root bob / world offset),
   - `preMultiplyLocalJoint` — left-multiply one joint’s local transform (aim/IK-style correction before FK).
2. Cover behavior with `animation_blend_test`.

## Consequences

- Positive: a single place to extend with quaternion blend, additive layers, and mask tables.
- Positive: post-process hooks are explicit and testable without a physics or IK middleware dependency.
- Trade-off: no blend trees, state machines, or per-bone blend masks yet (chunk 036+ / future ADRs).
- Follow-up: quaternion slerp tracks, bone masks, locomotion phase matching, and constraint solvers.

## Alternatives considered

- **Blend only at clip sampling time:** rejected because layering procedural passes after sampled clips matches common engine graphs (clip → modifiers → FK).
