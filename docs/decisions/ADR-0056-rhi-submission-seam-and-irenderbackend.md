# ADR-0056: RHI submission seam (`MeshDrawInstance`, `IRenderBackend`)

## Status

Accepted (baseline).

## Context

`VulkanRhi` was the only graphics path and owned both Vulkan details and the struct passed into `drawFrame`. A second sample, alternative backend, or slimmer game layer needs **shared draw submission types** and a **narrow interface** without pulling Vulkan headers into gameplay.

## Decision

1. **`marble::render::MeshDrawInstance`** lives in [`engine/render/RenderTypes.hpp`](../../engine/render/RenderTypes.hpp): mesh index, model matrix, per-instance color. Games and `IRenderPhase` implementations build arrays of this type.
2. **`marble::render::IRenderBackend`** in [`engine/render/IRenderBackend.hpp`](../../engine/render/IRenderBackend.hpp) exposes `drawFrame(Window&, Mat4 const& viewProj, span<MeshDrawInstance const>)` and `initialized()`.
3. **`VulkanRhi`** implements `IRenderBackend`, keeps Vulkan-only methods (`init`, `initFromSpirvBytes`, `uploadMesh`, `setClearColor`, `shutdown`), and aliases `DrawCommand` to `MeshDrawInstance` for existing call sites.

## Consequences

- Positive: Clear seam between “what to draw” and “how Vulkan records it”; tests can target pure logic without a GPU.
- Positive: Future D3D/Metal/null backends can implement the same interface.
- Trade-off: `IRenderBackend` is minimal on purpose—resource creation and pipeline selection remain on concrete types until a broader render graph exists.

## Follow-up actions

- Revisit thickness of `VulkanRhi` when adding materials, passes, or bindless resources; extend or split interfaces as needed.
- Align with [`OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) RHI row when a second backend is planned.
