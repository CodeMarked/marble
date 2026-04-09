# ADR-0049: Runtime object model generational store baseline (Chapter 16 -> §16.2)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 049** in **Chapter 16 Runtime Gameplay Foundation Systems** through **§16.2 Runtime Object Model Architectures**. After the gameplay-world/editor seam from chunk 048, Marble needs a stable runtime object identity model that avoids dangling references and supports packed fixed-capacity storage.

## Decision

1. Add [`gameplay/RuntimeObjectModel.hpp`](../../engine/gameplay/RuntimeObjectModel.hpp):
   - `ObjectHandle` (`index`, `generation`) and `RuntimeObject`.
   - `RuntimeObjectStore<MaxObjects>` fixed-capacity store with:
     - free-list based slot reuse,
     - `create`, `destroy`, `isAlive`, `get/getMutable`,
     - `forEachAlive`,
     - generation bump on destroy to invalidate stale handles.
2. Verify with `runtime_object_model_test`.

## Consequences

- Positive: handle validity survives slot reuse and prevents use-after-destroy bugs in gameplay code.
- Positive: fixed-capacity storage preserves deterministic allocation behavior.
- Trade-off: no component graph, no cross-world references, no persistence format binding yet.
- Follow-up: object model expansion in subsequent chapter slices (chunk 050+).

## Alternatives considered

- **Raw pointers as runtime identities:** rejected; stale pointer hazards conflict with deterministic runtime goals.
- **Heap-allocation per object:** rejected; fixed-capacity model is simpler and deterministic at this stage.
