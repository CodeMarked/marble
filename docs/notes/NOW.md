# Now

## Current focus

- **Shipped on `dev`:** first **Vulkan** presentation path (`VulkanRhi`) and the **`marbles`** sample (tilt board, fixed-step sim, draw via `IRenderPhase`). GitHub Actions builds with **vcpkg** (Vulkan + **shaderc**/ `glslc`). Mesh SPIR-V is staged under **`assets/shaders/`** as well as **`shaders/`**; when the engine resolves an assets root, `marbles` loads shaders through **`BinaryResourceManager`** + **`VulkanRhi::initFromSpirvBytes`** (directory `init` remains fallback).
- **Platform input:** `PlatformKeyboardBridge` + expanded `Window` keys, **`PlatformGamepadBridge`** (first GLFW gamepad into `AbstractControl`, including stick-as-D-pad for existing remaps), and **`ActionPolicy::applyActionContext`** / `resetActionGates` for batched ownership/disable transitions; `marbles` applies gameplay vs victory contexts and still reads quit after a win.
- **`docs/`** is **local-only** (not in the public git tree). Code comments may still cite `docs/decisions/*` for maintainers who keep that tree beside the repo.
- **ADR-0055** records the Vulkan bootstrap + repo/docs boundary; **shader tooling** and **RHI depth** remain open (see [`OPEN_QUESTIONS.md`](OPEN_QUESTIONS.md)).
- Chunk study and ADR-0001–0054 policy headers remain the long-range architecture spine; immediate engineering gates are **shader pipeline choice**, **RHI evolution**, and **input follow-up** (multi-player pads, disconnect UX, richer UI contexts).
- **Multiplayer:** client-side **snapshot interpolation** is in code as [`SnapshotInterpolator.hpp`](../../engine/gameplay/SnapshotInterpolator.hpp) + `snapshot_interpolator_test`; next multiplayer ROI per [`MASTER_PLAN.md`](../MASTER_PLAN.md) §4 is **interest management**, **benchmark harness + metrics**, then prediction/correction and visual wiring (see [`architecture/multiplayer-physics-world-scale.md`](../architecture/multiplayer-physics-world-scale.md) item 7–9).

## Next actions

Canonical ordering and themes: [`MASTER_PLAN.md`](../MASTER_PLAN.md) §4 *Roadmap*.

1. **Rendering (RHI and shaders):** advance the **shader compilation** and **`VulkanRhi` thickness** rows in [`OPEN_QUESTIONS.md`](OPEN_QUESTIONS.md) when implementing or planning the next RHI slice; keep ADR-0057/0056 baselines as the floor until decisions land. Harden Vulkan as pain points appear (resize, validation layers policy, device selection UX).
2. **Assets and ACP:** when pushing the resource-manager milestone, pull the **intermediate asset formats** open question into the same blocking set (see OPEN_QUESTIONS **Current roadmap**).
3. **Input:** multi-slot gamepads, reconnect, optional tests in [`runbooks/adr-test-traceability.md`](../runbooks/adr-test-traceability.md) for ADR-0027–0030.
4. Refresh [`runbooks/testing.md`](../runbooks/testing.md) and the root README if the documented CTest sample list drifts from [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt).
5. If onboarding external contributors, decide what **public** architecture surface to add (never raw copyrighted PDFs); **docs publication** and **vcpkg vs Vulkan SDK** rows in OPEN_QUESTIONS apply.

## Risks and unknowns

- Over-committing to `VulkanRhi` shape before the **libshaderc** vs CLI vs offline SPIR-V decision.
- Drift between **local** `docs/` and **public** README unless `MASTER_PLAN` / runbooks are synced periodically.
- Deferred multiprocessor loop and gameplay boundary questions still apply as rendering workload grows.
