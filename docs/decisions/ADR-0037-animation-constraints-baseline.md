# ADR-0037: Animation constraints baseline (Chapter 12 §12.11)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 037** in **§12.11 Constraints**. Gameplay animation relies on **geometric limits** (reach, contact projection) and **multi-bone IK** to satisfy end-effector goals while respecting bone lengths.

## Decision

1. Add `[animation/Constraints.hpp](../../engine/animation/Constraints.hpp)` with:
   - `closestPointOnSegment` for projection-style constraints (contact, poles),
   - `clampReachTarget` to clamp an effector goal onto a sphere (ball-and-socket reach),
   - `solveTwoBoneMidJoint` for a **planar two-bone IK** solution: projects the goal onto the reachable distance range, builds a plane from `(target - root)` and a **bend-hint** pole vector, and returns the middle joint position (optionally the adjusted target).
2. Verify with `animation_constraints_test`.

## Consequences

- Positive: reusable math for foot placement, look targets, and simple limb IK without a full physics articulation dependency.
- Positive: composes with existing `Vec3` / `Point3` types (ADR-0017).
- Trade-off: no full CCD/FABRIK, angular limits, or constraint priority stacks yet.
- Follow-up: joint limit cones, multi-chain IK, stabilization (damping), and integration with `preMultiplyLocalJoint` / skeleton bind poses.

## Alternatives considered

- **External IK middleware only:** deferred; these helpers stay deterministic for unit tests and headless builds.
- **Immediate quaternion joint output:** deferred; positions are enough to validate the geometry and can drive rotation construction later.
