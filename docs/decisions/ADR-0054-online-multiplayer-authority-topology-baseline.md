# ADR-0054: Online multiplayer authority/topology baseline (Chapter 17 -> §17.2)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 054** in **Chapter 17 You Mean There's More?** through **§17.2 Gameplay Systems**. Marble now has runtime object updates, events, and game-flow seams, and needs a concrete baseline for robust online multiplayer topology choices (offline, listen-host, and dedicated server). This chunk originally deferred **third-party transport middleware**; the engine now also ships a **thin in-tree UDP seam** and wire/session types ([`ADR-0060`](ADR-0060-multiplayer-data-plane-and-authority-constraints.md), [`GameTransport.hpp`](../../engine/gameplay/GameTransport.hpp)) without changing the **topology contract** in [`OnlineMultiplayerFoundation.hpp`](../../engine/gameplay/OnlineMultiplayerFoundation.hpp).

## Decision

1. Add [`gameplay/OnlineMultiplayerFoundation.hpp`](../../engine/gameplay/OnlineMultiplayerFoundation.hpp):
   - `SessionConfig` + `isValid` for topology/tick-rate constraints.
   - `ConnectionState` + `connectionTransitionAllowed` state machine.
   - `AuthorityRoster<MaxPlayers>` for host authority tracking and authenticated peer lifecycle.
   - `InputCommandQueue<Capacity>` for fixed-capacity queued input frames.
   - `ReplicationMessageMeta` + `requiresAck` for reliable/control-channel policy.
2. Support three authority topologies in one contract:
   - `Offline` (single-player only),
   - `ListenServer` (local host also a player),
   - `DedicatedServer` (no local player authority slot by default).
3. Keep this baseline deterministic and allocation-free where possible; **full** rollback, matchmaking, and **game-loop–integrated** replication are still follow-up. **Serialization and a UDP `IGameTransport` implementation** are implemented separately ([`ADR-0060`](ADR-0060-multiplayer-data-plane-and-authority-constraints.md)); roster/session types here remain usable without linking those headers.
4. Verify with `online_multiplayer_foundation_test`.

## Consequences

- Positive: multiplayer/session architecture has a testable contract that can scale from self-host to dedicated-server deployments.
- Positive: authority and peer-lifecycle rules are explicit, preventing ad-hoc host/client branching.
- Trade-off: payload schemas in this header are still **policy and small structs**; **on-wire** kinematics and session envelopes live in [`MultiplayerWireFormat.hpp`](../../engine/gameplay/MultiplayerWireFormat.hpp) / [`MultiplayerSessionEnvelope.hpp`](../../engine/gameplay/MultiplayerSessionEnvelope.hpp).
- Follow-up: additional transport adapters (Steam/EOS/etc.), authentication/session discovery, snapshot **delta** compression, server reconciliation/rollback, and **wiring** `AuthorityRoster` / `SessionConfig` to a headless authoritative tick. Data-plane rules (single physics authority, interest management, control vs tick traffic) are captured in [`ADR-0060`](ADR-0060-multiplayer-data-plane-and-authority-constraints.md). Baseline UDP + tests + `mp_foundation` sample: see [`ADR-0060` follow-up](ADR-0060-multiplayer-data-plane-and-authority-constraints.md#follow-up-actions).

## Alternatives considered

- **Implement full networking stack now:** rejected; this chunk sets architecture seams first.
- **Separate contracts per topology:** rejected; shared contract reduces drift between offline/listen/dedicated behavior.
