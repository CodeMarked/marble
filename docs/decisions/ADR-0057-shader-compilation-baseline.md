# ADR-0057: Shader compilation baseline (`glslc` CLI, SPIR-V at build time)

## Status

Accepted (narrows open question; long-term ACP may extend).

## Context

[`ADR-0055`](ADR-0055-vulkan-first-rhi-and-repository-docs-boundary.md) established GLSL → SPIR-V via the **`glslc`** CLI for the `marbles` sample, with CI obtaining `glslc` through vcpkg **shaderc**. Alternatives (runtime **libshaderc**, checked-in `.spv` only, **glslang** flows) remain listed in [`OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md).

## Decision

1. **Baseline for Marble targets:** Keep **offline compilation** with **`glslc`** invoked from CMake (`game/CMakeLists.txt`). Outputs are staged next to the game executable (`shaders/*.spv`) **and** copied into `assets/shaders/` beside the executable so the same blobs load through `BinaryResourceManager` virtual paths (`shaders/mesh.vert.spv`, etc.). This matches current developer (Vulkan SDK) and CI (vcpkg hints) workflows.
2. **No runtime shader compilation** in the engine baseline; SPIR-V is consumed as **precompiled bytes**. `VulkanRhi::init` reads them from a directory; `VulkanRhi::initFromSpirvBytes` accepts in-memory spans (used by `marbles` when the runtime assets root resolves and shaders were acquired via `BinaryResourceManager`).
3. **Future work** (separate ADR when ACP matures): optional libshaderc for hot reload, precompiled SPIR-V blobs in packs, or cross-compilation in a dedicated tools target.

## Consequences

- Positive: Reproducible builds, simple dependency story, no shader compiler linked into `marbles.exe`.
- Trade-off: Iteration requires rebuild or dev-loop script; changing strategy needs CMake and possibly CI updates.

## Follow-up actions

- If hot reload or user-generated shaders appear, revisit with **chunk 030** / resource-manager context and update [`OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) shader row to Closed with superseding ADR.
