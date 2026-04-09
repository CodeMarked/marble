# ADR-0052: Events and message-passing baseline (Chapter 16 §16.8)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 052** in **§16.8 Events and Message-Passing**. Marble has runtime object and update seams, but lacks a central event channel for decoupled gameplay communication.

## Decision

1. Add [`gameplay/EventsAndMessaging.hpp`](../../engine/gameplay/EventsAndMessaging.hpp):
   - `EventBus<MaxSubscriptions, MaxQueuedEvents>` fixed-capacity queue + subscriber list.
   - `EventMessage` (`StringId` + scalar payload) and native `EventHandler` callback shape.
   - `subscribe`, `unsubscribe`, `publish`, and `dispatchAll` operations.
2. Keep the baseline deterministic and allocation-free:
   - events are dispatched in queued order,
   - subscribers are matched by event id,
   - capacity limits fail explicitly via boolean return values.
3. Verify with `events_and_messaging_test`.

## Consequences

- Positive: gameplay systems can communicate without direct dependencies.
- Positive: event flow is testable and deterministic under fixed capacities.
- Trade-off: payload is scalar and untyped; richer payload schemas are deferred.
- Follow-up: integrate event bus with scripting/game-flow transitions and object update phases.

## Alternatives considered

- **Immediate dynamic/heap-backed event containers:** rejected; this stage keeps deterministic fixed-capacity behavior.
- **Per-system direct callbacks only:** rejected; point-to-point wiring scales poorly compared with a shared message channel.
