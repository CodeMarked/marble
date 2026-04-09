# ADR-0055: Vulkan-first RHI, marbles demo, and repository vs local docs boundary

## Status

Accepted (baseline); see **Follow-up actions** and [`notes/OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) for unresolved tooling and RHI-depth choices.

## Context

Marble needed a first end-to-end path from windowing through presentation so architecture headers and phase interfaces connect to real GPU work. At the same time, maintainer documentation (ADRs, chunk notes, book-aligned material) must stay **out of the public git tree** so the open repository stays lean and free of copyrighted third-party book PDFs. CI runners do not ship the full LunarG Vulkan SDK, so shader compilation must be bootstrapped there as well.

## Decision

1. **Graphics path (v1)**  
   - The engine ships a first implementation under `engine/render/vulkan/` (`VulkanRhi`): instance/device/swapchain, render pass, minimal pipeline, depth, synchronization, and staging helpers sufficient for the sample game.  
   - The **`marbles`** sample implements `IRenderPhase` and uses GLSL sources compiled to SPIR-V at **build time** via the **`glslc`** CLI (`--target-env=vulkan1.2`). CMake stages `.spv` files next to the executable **and** under `assets/shaders/` (alongside staged `assets/`). When `Engine::assetsRootPath()` resolves, `marbles` prefers loading SPIR-V through `BinaryResourceManager` + `VulkanRhi::initFromSpirvBytes`, with directory-based `init` as fallback.

2. **Repository boundary**  
   - The **`docs/`** tree (ADRs, architecture notes, book chunks, runbooks as maintained locally) is **not tracked in git** (see repo root `.gitignore`). The public repo carries **README + code + tests + CI** only.  
   - **Copyrighted book PDFs** and similar third-party full texts **must not** be committed to git.

3. **Tooling: developers vs CI**  
   - **Local development:** LunarG **Vulkan SDK** is supported as the primary way to obtain `glslc` (`VULKAN_SDK` / PATH).  
   - **CI (GitHub Actions):** **vcpkg** installs `vulkan-headers`, `vulkan-loader`, and **`shaderc`** (provides `glslc`); `PATH` is updated so `find_program(glslc)` succeeds.  
   - **CMake:** `game/CMakeLists.txt` passes `find_program` hints for `VULKAN_SDK` and, when using the vcpkg toolchain, `…/installed/<triplet>/tools/shaderc`.

## Consequences

- Positive: One working Vulkan path; reproducible CI without a SDK installer; clear split between public code and private study/docs.  
- Trade-offs: Cloners without a local `docs/` checkout rely on README and source; cross-references in comments to `docs/decisions/*` assume maintainers keep that tree. Shader compilation is tied to **`glslc` availability** until a future ADR changes the pipeline.

## Alternatives considered

- **ImGui / third-party renderer:** Rejected for the stated goal of owning the Vulkan path.  
- **Commit `docs/` to GitHub:** Rejected to avoid leaking copyrighted PDFs and to keep the public repo minimal.  
- **Require LunarG SDK in CI only:** Rejected as fragile on hosted runners; vcpkg `shaderc` is scriptable.

## Follow-up actions

- Long-term **shader compilation** beyond the `glslc` baseline: see [`ADR-0057`](ADR-0057-shader-compilation-baseline.md) and [`notes/OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md).  
- **RHI thickness** baseline: [`ADR-0056`](ADR-0056-rhi-submission-seam-and-irenderbackend.md); revisit when a second backend or second game appears.  
- Keep [`notes/NOW.md`](../notes/NOW.md) and [`MASTER_PLAN.md`](../MASTER_PLAN.md) aligned with shipped render status.
