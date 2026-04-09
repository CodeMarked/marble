# ADR-0048: Gameplay runtime and world editor foundation (Chapter 15 -> §15.4)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 048** in **Chapter 15 Introduction to Gameplay Systems** through **§15.4 The Game World Editor**. Marble needs a small runtime seam that can host gameplay objects while supporting editor-driven operations without tying directly to rendering or persistence systems.

## Decision

1. Add [`gameplay/GameplayFoundation.hpp`](../../engine/gameplay/GameplayFoundation.hpp):
   - `GameplayObject` and `ObjectId` for runtime object state (`archetypeId`, transform position, enabled flag).
   - `GameplayWorld<MaxObjects, MaxCommands>` fixed-capacity world with spawn/update, destroy, lookup, active iteration.
   - `EditorCommand` queue (`AddObject`, `RemoveObject`, `MoveObject`, `ToggleEnabled`) and `applyQueuedEditorCommands`.
2. Keep editor and runtime decoupled through command buffering:
   - gameplay/editor code enqueues commands,
   - world applies commands in a deterministic batch.
3. Verify with `gameplay_foundation_test`.

## Consequences

- Positive: clear seam for gameplay object state before larger runtime object model work.
- Positive: editor actions can be staged and applied deterministically.
- Trade-off: no hierarchy/components/serialization yet.
- Follow-up: chunk 049+ will expand into richer runtime object models and data formats.

## Alternatives considered

- **Adopt full ECS/editor integration now:** rejected; this chunk is an architecture baseline, not a full toolchain integration.
- **Immediate mode editor mutations only:** rejected; command queue helps preserve deterministic update boundaries.
