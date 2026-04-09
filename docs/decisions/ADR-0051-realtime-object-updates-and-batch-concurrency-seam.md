# ADR-0051: Real-time object updates and batch-concurrency seam (Chapter 16 §16.6–§16.7)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 051** in **§16.6 Updating Game Objects in Real Time** through **§16.7 Applying Concurrency to Game Object Updates**. Marble has runtime object identities and world data baselines, but needs a clear update seam that can run deterministically now and map to parallel jobs later.

## Decision

1. Add [`gameplay/RealtimeObjectUpdates.hpp`](../../engine/gameplay/RealtimeObjectUpdates.hpp):
   - `UpdateContext` (delta + gravity-like acceleration term).
   - `collectAliveHandles` to snapshot update candidates.
   - `integrateObjectPositions` applying simple position integration for enabled objects.
   - `buildUpdateWorkRanges` using existing `core::job::scatterGather` to define batch ranges.
2. Keep policy deterministic and backend-agnostic:
   - updates are ordered over captured handles,
   - batching is explicit and can be mapped to worker jobs in future schedulers.
3. Verify with `realtime_object_updates_test`.

## Consequences

- Positive: gameplay update phase has a testable seam aligned with future concurrency rollout.
- Positive: handle collection decouples traversal from update execution policy.
- Trade-off: no dependency graph/topological ordering or conflict resolution yet.
- Follow-up: integrate with message/event systems and real worker-backed execution.

## Alternatives considered

- **Immediate direct iteration + update only:** rejected; explicit batch metadata is needed for §16.7 concurrency evolution.
- **Thread pool integration now:** deferred; keep this chunk dependency-free and deterministic.
