# ADR-0032: Skeleton parent ordering and local→global pose baseline (Chapter 12 §12.1–§12.3)

## Status

Accepted

## Context

`[docs/book/chunks/index.md](../book/chunks/index.md)` places **chunk 032** in **Chapter 12 — Animation Systems** through **§12.3 Poses**. Before clips, blending, and skinning, the engine needs a minimal, testable representation of a skeletal hierarchy and how **local joint transforms** compose into **global (model-space) poses** via forward kinematics.

## Decision

1. Add `[animation/SkeletonPose.hpp](../../engine/animation/SkeletonPose.hpp)` under `marble::animation`:
  - `kInvalidParent` sentinel for the root joint (index `0`),
  - `skeletonParentsWellOrdered` to validate a **single root** and **parent index strictly less than child index** (depth-first / topological storage),
  - `computeGlobalPose` implementing one-pass FK: `global[i] = global[parent[i]] * local[i]`.
2. Use existing `[math/Mat4.hpp](../../engine/math/Mat4.hpp)` for local/global transforms (ADR-0018 conventions).
3. Cover policy with `animation_pose_test` (valid/invalid parent layouts and a short FK chain).

## Consequences

- Positive: animation code has a clear seam for future clip sampling and skinning matrix palettes.
- Positive: invalid parent tables are detectable before FK without silent garbage.
- Trade-off: only single-root, well-ordered skeletons are supported; arbitrary graphs and runtime retargeting are out of scope.
- Follow-up: joint names / bind pose, inverse bind matrices, clip timelines (chunk 033+), and authoring format import.

## Alternatives considered

- **Recursive FK:** rejected for the baseline API in favor of a single linear pass under the stated ordering constraint.
- **Full scene skeleton asset:** deferred until resource pipeline work targets animation data.

