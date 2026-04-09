# ADR-0060: Multiplayer data plane and authority constraints

## Status

Accepted

## Context

[`ADR-0054`](ADR-0054-online-multiplayer-authority-topology-baseline.md) defines session topology and authority **seams** without transport. Real internet play, physics (e.g. Jolt via [`ADR-0058`](ADR-0058-physics-middleware-integration.md)), and large worlds ([`ADR-0059`](ADR-0059-simulation-space-and-physics-tier-contract.md)) need **non-negotiable rules** so we do not redesign core architecture later due to bandwidth, determinism, or sim ownership mistakes.

## Decision

1. **Control plane vs data plane**  
   **HTTPS / REST / WebSocket** services (e.g. account, login, lobby, economy, match metadata) are **separate** from the **real-time game data plane** (tick rate, physics, world state). The **game simulation process** (listen host or dedicated server) owns **high-frequency** state; platform APIs **issue credentials or join metadata**, not per-tick physics.

2. **Authoritative physics**  
   **One** simulation authority owns **middleware / rigid-body truth** for a session. Other peers **do not** assume **bit-identical** float physics across machines. Clients may **predict or interpolate**; reconciliation follows an explicit policy. A **thin UDP + framing baseline** now exists in-tree ([`GameTransport.hpp`](../../engine/gameplay/GameTransport.hpp), [`UdpGameTransport.hpp`](../../engine/gameplay/UdpGameTransport.hpp), [`MultiplayerSessionEnvelope.hpp`](../../engine/gameplay/MultiplayerSessionEnvelope.hpp)); **client prediction/interpolation policy and implementation** remain to be specified and built ([`multiplayer-physics-world-scale.md`](../architecture/multiplayer-physics-world-scale.md)).

3. **Bounded high-fidelity sim**  
   Full contact resolution applies only to a **budgeted** set of bodies near active play ([`ADR-0059`](ADR-0059-simulation-space-and-physics-tier-contract.md) tier intent). Distant or low-priority entities use **cheaper** representations.

4. **Interest management**  
   Replication must be **scoped** (who receives which entity updates at which rate). Designing **entity IDs, regions, and bandwidth budgets** is mandatory before scaling player counts.

5. **Authoritative time**  
   The authority uses **fixed simulation step** semantics aligned with [`ADR-0003`](ADR-0003-fixed-step-simulation-phase.md) on the server/host side; network stalls are handled with a **documented** policy (catch-up cap, drop, etc.) when transport exists.

6. **Streaming / thin clients**  
   Clients that receive **video** and send **input only** still **attach to the same authority model**; they are a **delivery** variant, not a second conflicting sim.

7. **Transport choice is pluggable**  
   Prefer a **thin engine seam** (e.g. UDP library behind an interface). **Default direction** for a first implementation: **dedicated or listen host with public reachability** → simple UDP framing; add **NAT traversal / relay** (or a higher-level library) when home hosting must reach arbitrary internet clients.

## Consequences

- Positive: avoids planning for impossible **global lockstep Jolt** and keeps **platform services** scalable on familiar HTTP stacks.
- Positive: keeps **Marble engine** ownership of **sim + physics** clear.
- Trade-off: **first UDP/datagram path + session envelope + kinematics wire types** are implemented as a **narrow** baseline; this ADR does **not** by itself deliver production netcode (sequencing/acks for control traffic, interest management, delta compression, NAT traversal, full authoritative tick loop in a shipped game sample).
- Trade-off: **strict lockstep** is **not** assumed for physics; deterministic subsystems (if any) must be **explicitly** scoped.

## Alternatives considered

- **Lockstep float physics on all peers:** rejected as default; cross-CPU / middleware nondeterminism makes it fragile at scale.
- **Run tick + physics inside HTTP handlers (e.g. Gin):** rejected for game data plane; wrong latency and coupling model.

## Follow-up actions

- **Done (baseline):** [`IGameTransport`](../../engine/gameplay/GameTransport.hpp) + loopback pair; [`UdpGameTransport`](../../engine/gameplay/UdpGameTransport.hpp) + [`UdpSocket`](../../engine/platform/network/UdpSocket.hpp); [`MultiplayerWireFormat.hpp`](../../engine/gameplay/MultiplayerWireFormat.hpp); [`MultiplayerSessionEnvelope.hpp`](../../engine/gameplay/MultiplayerSessionEnvelope.hpp); CTests + `mp_foundation` sample ([`game/mp_foundation/MpFoundationMain.cpp`](../../game/mp_foundation/MpFoundationMain.cpp)). See [`adr-test-traceability.md`](../runbooks/adr-test-traceability.md) ADR-0060 row.
- **Next:** reliable **control** messages on UDP (sequence, ack, retransmit); client **snapshot buffer + interpolation** (+ optional prediction); **interest management** and bandwidth budgets; **headless authority + synthetic clients** with impairment and **exported metrics** (RTT, bytes/s, loss). Ordered detail: [`multiplayer-physics-world-scale.md`](../architecture/multiplayer-physics-world-scale.md) § *Next engineering steps*.
- Extend to **internet** (NAT traversal / relay) when home hosting must reach arbitrary clients.
- Optional: platform **join / session token** HTTP API (may live outside this repo).
- Extend [`ADR-0054`](ADR-0054-online-multiplayer-authority-topology-baseline.md) traceability as snapshots and game-loop integration deepen.
- See [`docs/architecture/multiplayer-and-platform-baseline.md`](../architecture/multiplayer-and-platform-baseline.md) for a short backlog summary.
- Design reference: **replication taxonomy**, **velocity-aware interest**, **event bandwidth caps** — [`docs/architecture/multiplayer-physics-world-scale.md`](../architecture/multiplayer-physics-world-scale.md).
