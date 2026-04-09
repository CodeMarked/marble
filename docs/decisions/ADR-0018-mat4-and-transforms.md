# ADR-0018: `Mat4`, homogeneous vectors, and basic transforms (Chapter §5.3)

## Status

Accepted

## Context

Chapter **§5.3** covers **matrices** for games: representation, **matrix–vector** and **matrix–matrix** multiplication, **transforms** (translation, rotation, scale), **affine** vs **projective** use of **homogeneous coordinates**, and order-of-operations pitfalls (row vs column vectors, multiply order).

[`ADR-0017`](ADR-0017-three-d-math-conventions-and-vec3.md) fixed **world handedness** and **`Vec3`**; rendering and simulation still need a **single agreed matrix convention** before camera/projection work.

## Decision

1. Add [`marble::math::Mat4`](../../engine/math/Mat4.hpp) with **column-major** storage (`m[col * 4 + row]`) and **column-vector** transforms: `v' = M * v` (matches common graphics APIs and keeps one multiply order rule).
2. Add minimal [`marble::math::Vec4`](../../engine/math/Mat4.hpp) for homogeneous **column** vectors.
3. Provide **identity**, **translation**, **non-uniform scaling**, and **rotation about +Y** (`rotationY`, radians) for the **left-handed** +Y-up frame in ADR-0017 (yaw moves **+X** toward **+Z**).
4. Provide **`transformPoint`** (`w = 1`, optional **perspective divide** if `w' ≠ 1`) and **`transformDirection`** (`w = 0`, **linear** part only; no re-normalization).
5. **Scene graph**, **quaternions**, **full Euler** decomposition, **inverse** / **transpose** shortcuts, and **projection** matrices are **deferred** until a concrete render/camera chunk; this chunk only grounds **§5.3** conventions and **affine** composition tests.
6. **No third-party math library** yet; revisit when multiple subsystems need identical edge-case behavior (see `vec3_math_test` gap in traceability).

## Consequences

- Positive: one place for **column-major** / **column-vector** rules before Vulkan math bridges.
- Positive: `mat4_math_test` locks **translation**, **scale**, **yaw**, **direction vs point**, and **multiply order** `(R*T)*p`.
- Trade-off: **rotationY** only; add **rotationX/Z** or **axis–angle** when first camera or character controller needs them.

## Alternatives considered

- **Row-major storage:** rejected to match common **GLSL**/`mat4` column-major expectations and reduce future shader upload surprises.
- **GLM immediately:** deferred per ADR-0017 footprint rationale; can wrap or replace later.
