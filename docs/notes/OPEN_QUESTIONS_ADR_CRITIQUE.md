# Open questions × ADR critique (cross-walk)

This note applies the rubric in [`runbooks/adr-critique-rubric.md`](../runbooks/adr-critique-rubric.md) to each row in [`OPEN_QUESTIONS.md`](OPEN_QUESTIONS.md), with anchors in [`runbooks/adr-test-traceability.md`](../runbooks/adr-test-traceability.md) where rows exist.

**Legend — verdict on the open question**

| Verdict | Meaning |
| --- | --- |
| **Defer** | Wait for the listed book-chunk gate or a second runtime consumer before locking. |
| **Baseline** | An accepted ADR narrows scope; long-term question stays open until superseding ADR. |
| **Amend** | Existing ADR text/traceability should be updated (not necessarily the whole decision). |
| **New ADR** | Ready to draft a new ADR when implementation forces a cut. |
| **Close (OQ)** | Open-question row can move to Closed with ADR id (none of the current *Open* rows meet this bar yet). |

## Summary matrix

| Open question (abridged) | Primary ADRs | Secondary ADRs | Traceability rows | Verdict |
| --- | --- | --- | --- | --- |
| Engine core vs gameplay boundary | ADR-0048, ADR-0049 | ADR-0002, ADR-0053 | 0048/0049 Good (policy tests); 0002 Partial | Defer |
| Simulation phase / authority (multiplayer) | ADR-0054 | ADR-0002, ADR-0052 | 0054 Good | **Closed** (see §2) |
| Fixed-step under severe stall | ADR-0003 | ADR-0025 | 0003 Good; 0025 Good | Amend + New ADR (when chunk 025) |
| Concurrency default | ADR-0001, ADR-0011 | ADR-0026, ADR-0051 | 0001 Partial; 0011/0026/0051 Good | Defer |
| Data ownership | ADR-0024 | ADR-0020, ADR-0049, ADR-0010 | 0024 Good; others policy | Defer |
| Profiling gates | — | ADR-0005 | 0005 Partial | Defer (no ADR yet) |
| ACP / intermediate formats | ADR-0024 | ADR-0023, ADR-0057 | 0024 Good | Defer + New ADR (with tooling) |
| Shader compilation (long-term) | ADR-0057 | ADR-0055 | 0055 Partial; **0057 see traceability** | Baseline |
| VulkanRhi thickness | ADR-0056 | ADR-0055 | 0055 Partial; **0056 see traceability** | Baseline |
| Publish `docs/` | ADR-0055 | — | 0055 (process in Context) | Defer (process) |
| vcpkg vs Vulkan SDK | ADR-0055 | ADR-0057 | 0055 Partial | Defer (operational) |

---

## 1. Final subsystem boundary: engine core vs gameplay runtime

**Verdict:** **Defer** until chunk **049** and until a second game or shared runtime forces API splits.

**ADR critique**

- **ADR-0048 / ADR-0049:** Clear **Decision** (gameplay foundations, generational store). **Status** honest as policy baselines. **Traceability:** Good header-level tests; **no** end-to-end “only gameplay may call X” enforcement in CI—expected at this maturity.
- **ADR-0002:** Defines phase interfaces; **does not** draw engine/game package boundary—correct scope.

**Contradiction / gap:** [`MASTER_PLAN.md`](../MASTER_PLAN.md) describes wide policy shipment vs narrow runtime wiring; the open question asks for **final** boundaries—ADR set supports *interfaces* more than *ownership rules*. No conflict; just incomplete for “final.”

**Next step:** Draft a short boundary ADR only when `marbles` (or a second sample) duplicates logic that should live only in `engine/`.

---

## 2. Concrete simulation phase contract (offline / self-host / central server)

**Verdict:** **Close (OQ row)** — already **Closed** in `OPEN_QUESTIONS.md` with **ADR-0054**.

**ADR critique**

- **ADR-0054:** Strong **Decision** (session config, FSM, authority roster, inputs, ack policy). **Alternatives** implied via topology enum. **Traceability:** Good (`online_multiplayer_foundation_test`); remaining gaps are transport/real net—appropriately out of scope for the baseline.

**Regression check:** Keep row Closed; no OPEN_QUESTIONS edit required beyond current state.

---

## 3. Fixed-step simulation under severe frame stalls

**Verdict:** **Amend** ADR-0003 follow-ups when chunk **025** arrives; optionally **New ADR** if policy splits from “fixed step” (e.g. catch-up budget as a separate concern).

**ADR critique**

- **ADR-0003:** Explicit **Decision** (accumulator, cap, drop excess). **Alternatives considered** present. **Open-question alignment:** The open question asks for *production* stall policy nuance; ADR-0003 already **chooses** drop behavior (“drop excess accumulated time”)—partial answer. **Follow-up actions** explicitly ask for configurable drop vs catch-up—**good honesty**.
- **ADR-0025:** Frame-delta *measurement* policy; does not replace stall handling—orthogonal.

**Traceability:** `fixed_step_simulation_test` validates baseline stepping; it does not exhaust “severe stall” product policy—acceptable **Partial** until diagnostics counters land (already noted in traceability for 0003).

**Next step:** When implementing, extend ADR-0003 or add ADR-00xx and add a targeted test for cap/drop vs catch-up toggle.

---

## 4. Concurrency model for first stable milestone

**Verdict:** **Defer** until chunk **011** / **014** and until workers exist.

**ADR critique**

- **ADR-0001:** Clear single-thread baseline; **Follow-up** defers parallelism—aligns with open question.
- **ADR-0011:** No worker threads; hints only—consistent.
- **ADR-0026 / ADR-0051:** Seams for jobs and scatter/gather without mandating threads—**narrow** the space but **do not** pick a default concurrency model for “stable milestone.”

**Contradiction:** None; avoid choosing thread pool vs task graph until there is measurable need (guardrail: profiling evidence).

**Next step:** Revisit when first subsystem uses `JobSystem` with real workers.

---

## 5. Data ownership across runtime systems

**Verdict:** **Defer** until chunk **020** and concrete cross-subsystem bugs or refactors.

**ADR critique**

- **ADR-0024:** Clear ownership of *binary resource* instances (registry + ref count). Does not define global rules for meshes vs transforms vs gameplay state—**omits** the breadth of the open question.
- **ADR-0049:** Object store ownership model for generational handles—relevant slice only.
- **ADR-0020 / ADR-0010:** Support ordering and false-sharing hygiene—supporting, not sufficient.

**Next step:** Consolidate into one ADR when multiple subsystems share mutable runtime state beyond current patterns.

---

## 6. Profiling gates before accepting core changes

**Verdict:** **Defer** until chunk **028**; **no primary ADR** today.

**ADR critique**

- **ADR-0005:** Diagnostics *interval* knob—related to observability, **not** acceptance gates. Traceability **Partial**; does not answer “what profiling gates.”

**Next step:** Add a lightweight ADR or CONTRIBUTING section when CI can run a repeatable perf smoke (even if manual threshold).

---

## 7. ACP: intermediate formats and conditioning

**Verdict:** **Defer** until chunk **024** tooling work; **New ADR** when first importer or pack file exists.

**ADR critique**

- **ADR-0024:** Defers streaming, dependency graphs, GPU stages—explicitly **narrow**; aligns with “ACP not decided.”
- **ADR-0057:** Shader slice of pipeline—feeds ACP thinking but does not define mesh/texture conditioning.

**Next step:** First concrete format on disk → ADR naming stages and CI expectations.

---

## 8. Long-term shader compilation story

**Verdict:** **Baseline** — **ADR-0057** fixes the *current* approach; OPEN_QUESTIONS row correctly stays **Open** for libshaderc / checked-in SPIR-V / glslang.

**ADR critique**

- **ADR-0057:** **Decision clarity** high; **Status** says baseline—honest. **Consequences** name iteration cost. **Alternatives** deferred to future ADR—appropriate.
- **ADR-0055:** Vulkan + `glslc` + CI vcpkg; **Partial** traceability; overlaps 0057—no conflict.

**Traceability:** See dedicated row for ADR-0057 in `adr-test-traceability.md` (build-time `glslc`; no dedicated CTest).

**Next step:** Close OPEN_QUESTIONS row only when a superseding ADR picks long-term strategy; until then keep “Open (baseline: ADR-0057).”

---

## 9. VulkanRhi thickness vs game draw/binding

**Verdict:** **Baseline** — **ADR-0056** states submission seam; thickness of *resources/pipelines* still open.

**ADR critique**

- **ADR-0056:** **Decision** minimal and testable in principle; **Consequences** admit `IRenderBackend` is thin—honest. **Open-question alignment:** **Narrows** “where draw lists live” but not full renderer architecture.
- **ADR-0055:** Positions Vulkan-first implementation; game still owns `IRenderPhase` wiring—consistent with 0056.

**Traceability:** No ADR-0056-specific automated test; **Partial** (compile/link + marbles runtime).

**Next step:** Second backend or material system → amend 0056 or add ADR for pass graph / resource binding ownership.

---

## 10. Publishing local `docs/` (license / redaction)

**Verdict:** **Defer** — process decision; **ADR-0055** **Context** records “public repo excludes local docs” but is **not** a substitute for a publish policy.

**ADR critique**

- **ADR-0055:** States repo boundary; **does not** decide whether to ever publish—correct omission for a graphics ADR.

**Next step:** Owner (project lead) decides publish/no-publish outside ADR-0055; optional short process ADR.

---

## 11. vcpkg everywhere vs Vulkan SDK default (vcpkg CI-only)

**Verdict:** **Defer** — operational; **ADR-0055** documents current **CI** path and local SDK story.

**ADR critique**

- **ADR-0055:** **Consequences** name trade-offs at high level; **does not** mandate one dev workflow—appropriate. **Traceability** ties to `.github/workflows` and CMake hints—**Partial** for “developer standardization” because that is not fully automatable in tests.

**Next step:** Team onboarding doc + optional CONTRIBUTING; ADR update only if policy becomes strict.

---

## Maintenance

- Re-run this cross-walk when adding ADRs **0058+**, changing render/tooling, or closing an OPEN_QUESTIONS row.
- Update [`adr-test-traceability.md`](../runbooks/adr-test-traceability.md) in the same change as new ADRs or materially new gaps.
