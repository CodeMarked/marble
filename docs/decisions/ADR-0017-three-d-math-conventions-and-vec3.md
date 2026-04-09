# ADR-0017: 3D math conventions and `Vec3` (Chapter 5 through §5.2)

## Status

Accepted

## Context

Chapter 5 introduces **3D math for games**, with **§5.1** on reducing 3D problems to 2D where possible, and **§5.2** on **points**, **vectors**, **Cartesian** and alternate coordinate systems, **handedness**, **basis vectors**, **vector operations** (scale, add/subtract, magnitude, normalization, dot, cross), **pseudovectors**, and **linear interpolation (LERP)**.

Marble needs a **small, explicit** math surface before matrices (chunk **018**) and a **documented world convention** so rendering and simulation agree later.

## Decision

1. Add [`marble::math::Vec3`](../../engine/math/Vec3.hpp) with component-wise operations, **dot**, **length** / **lengthSquared**, **cross**, **normalize**, and **lerp**, matching the textbook’s algebraic definitions (Gregory §5.2.4–5.2.5).
2. **World / graphics convention (left-handed):** **+X** right, **+Y** up, **+Z** forward into the scene (increasing depth away from the camera), matching the common **3D graphics** arrangement described in §5.2.2 (y up, x right, positive z along the view/depth direction). **Stick to this convention** in new engine code; if a subsystem (e.g. Vulkan NDC) uses a different frame, convert at a **bounded** boundary and document it.
3. **`Point3`** is a **type alias** of `Vec3` (same storage as typical game practice, §5.2.3). Preserve **semantic** distinction in API design: **point − point → direction**, **point + direction → point**; **point + point** remains invalid at the type level only by convention and review.
4. Prefer **squared length** for comparisons when possible (§5.2.4.3–5.2.4.4); avoid `sqrt` in hot paths unless necessary.
5. **Cross product** uses the **standard component formula** (§5.2.4.8); **handedness** affects **visualization** (right-hand vs left-hand rule), not a second formula in code (§5.2.2, §5.2.4.8). **Pseudovectors** / **exterior algebra** (§5.2.4.9) are **not** modeled in the type system; revisit only if a **handedness** or **reflection** conversion layer is added.
6. **Matrices** and **homogeneous coordinates** for **points and directions** are covered in [`ADR-0018`](ADR-0018-mat4-and-transforms.md) (`Mat4`, `Vec4`). **Quaternions**, **full scene graph**, and **projection** stacks remain **out of scope** until dedicated camera/render chunks.

## Consequences

- Positive: one place for 3D conventions and basic operations used by tests and future subsystems.
- Positive: `vec3_math_test` locks **dot**, **cross(i,j)=k**, **LERP**, and **distance-squared** patterns from the text.
- Trade-off: no `Vec2`/`Vec4` yet; add when a second call site needs them.

## Alternatives considered

- **Depend on GLM or similar immediately:** deferred to keep the engine footprint small until math needs stabilize.
- **Separate `Point` and `Vector` types:** deferred; alias + documentation matches common industry practice and the book’s note on libraries.
