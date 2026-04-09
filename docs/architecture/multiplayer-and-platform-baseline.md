# Multiplayer and platform baseline (summary)

Normative rules live in **[ADR-0060: Multiplayer data plane and authority constraints](../decisions/ADR-0060-multiplayer-data-plane-and-authority-constraints.md)**. This page is a **quick index** and **backlog** from recent planning.

**Dream-scale planning (non-normative):** [`multiplayer-physics-world-scale.md`](multiplayer-physics-world-scale.md) — replication taxonomy template, velocity-aware interest, tier handoff packets, water/huge-body defaults, explicit bytes/tick budgets.

**Engineering priorities (short):** UDP + binary wire + session envelope + `mp_foundation`, **authoritative/client session**, **reliable control**, and **decoded snapshot interpolation** (`SnapshotInterpolator.hpp`) are **landed**. **Next:** interest management (AOI, bytes/tick budgets), **headless benchmark** + exported metrics, wiring interpolation into a **visual** path, then prediction/correction—see [`multiplayer-physics-world-scale.md`](multiplayer-physics-world-scale.md) § *Next engineering steps* and [`MASTER_PLAN.md`](../MASTER_PLAN.md) §4 *Multiplayer server*.

## Tight constraints (do not violate casually)

- **One physics authority** per session; no assumed bit-identical Jolt across peers.
- **Control plane ≠ data plane:** login/match/economy on HTTPS (e.g. Go services); **game ticks** on a **C++ sim host** over a **real-time** protocol.
- **Cap high-fidelity physics** and **scope replication** (interest management + budgets).
- **Replication taxonomy:** per entity class, define owner, sim class, update rate, reliable vs unreliable channel, and loss behavior before scaling player count ([`multiplayer-physics-world-scale.md`](multiplayer-physics-world-scale.md)).
- **Fast movers:** plan for **velocity-aware** or swept interest and tier **handoff** messages (reliable) so [ADR-0059](../decisions/ADR-0059-simulation-space-and-physics-tier-contract.md) H1–H3 stay coherent across peers.
- **Fixed authoritative step** for the sim host; variable client frame rate is separate.
- **Coordinates:** follow [`ADR-0059`](../decisions/ADR-0059-simulation-space-and-physics-tier-contract.md) (local float island + double anchor); add **floating rebasing** when play space grows.

## Related ADRs

- [`ADR-0054`](../decisions/ADR-0054-online-multiplayer-authority-topology-baseline.md) — session topology, roster, queued inputs. **UDP transport + wire/session framing** exist in engine (`IGameTransport`, `UdpGameTransport`, `MultiplayerSessionEnvelope`); **full game-loop integration** (garden/marbles authoritative tick + replication) remains follow-up.
- [`ADR-0059`](../decisions/ADR-0059-simulation-space-and-physics-tier-contract.md) — simulation space, tier handoff invariants.

## Backlog (implementation and design)

| Area | Status / next work |
|------|---------------------|
| Transport | **Landed:** `IGameTransport`, loopback pair, `UdpGameTransport` + `UdpSocket`, session envelope (magic/version, reliable control + framed snapshots), **`mp_foundation`** running `AuthoritativeSession` / `ClientSession` over localhost UDP (raw peer discovery then session `tick`), CTests on localhost UDP; reliable control path (`ReliableChannel`, protocol v2). **Next:** NAT/relay when arbitrary internet clients matter. |
| Protocol depth | **Next:** optional delta compression; schema versioning beyond envelope `protocolVersion`; tier-handoff as a session message type when gameplay needs it. |
| Platform | Join/match endpoint returning address + session token (may be external to Marble). |
| Sim | **Landed:** `AuthoritativeSession` / `ClientSession` fixed-tick + snapshots; **interpolation helper** [`SnapshotInterpolator.hpp`](../../engine/gameplay/SnapshotInterpolator.hpp) (`snapshot_interpolator_test`). **Next:** headless dedicated binary; prediction + explicit reconciliation; game-loop wiring ([`multiplayer-physics-world-scale.md`](multiplayer-physics-world-scale.md) § *Next engineering steps*). |
| Scale | Interest management (velocity-aware AOI, bytes/tick caps); floating origin rebasing for `SimulationIsland`; streamed chunks / globe data (separate roadmap). |
| Design | [`multiplayer-physics-world-scale.md`](multiplayer-physics-world-scale.md): anchor/world replication contract, deterministic chunk+seed layout, event bandwidth caps, benchmark/metrics targets. |
| Clients | Native UDP to host; decode ring → [`SnapshotInterpolator`](../../engine/gameplay/SnapshotInterpolator.hpp) for presentation ticks. **Next:** render-clock mapping, sample wiring. Streaming clients = thin input/video path to **same** authority. |
| QA / stats | Headless authority + synthetic clients; impairment injection; export RTT, bandwidth, loss, correction metrics for CI or release notes. **Default:** deterministic **negative-path** tests on parsers; **defer** libFuzzer/AFL-style **fuzz** until reliable framing is stable ([`multiplayer-physics-world-scale.md`](multiplayer-physics-world-scale.md) § *Optional and deferred*). |
| Handshake UX | **`ClientSession`** resends `Hello` on a tick cadence (`kHelloRetryTicks`); **`mp_foundation`** client uses it. Optional follow-up: backoff tuning or shared UX helpers beyond the fixed retry interval. |
| Docs | Optional ADRs: reliable channel on UDP, snapshot delta format, determinism scope for static world. |
