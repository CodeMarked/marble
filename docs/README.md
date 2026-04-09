# Marble Documentation

Use this index to find the canonical source for each topic.

## Canonical sources

- Product state and priorities: [`MASTER_PLAN.md`](MASTER_PLAN.md)
- SDK vision, sample phases, and integrator expectations: [`architecture/sdk-and-samples-roadmap.md`](architecture/sdk-and-samples-roadmap.md)
- System design and boundaries: [`architecture/system-overview.md`](architecture/system-overview.md)
- Runtime and subsystem map: [`architecture/engine-subsystems.md`](architecture/engine-subsystems.md)
- Working agreements and constraints: [`guardrails/architecture-guardrails.md`](guardrails/architecture-guardrails.md)
- Development flow and verification: [`runbooks/local-dev.md`](runbooks/local-dev.md), [`runbooks/testing.md`](runbooks/testing.md) (CTest inventory and subsystem mapping)
- Decision-to-test traceability: [`runbooks/adr-test-traceability.md`](runbooks/adr-test-traceability.md)

## Planning and decisions

- Architecture decisions: [`decisions/`](decisions/)
- Open questions: [`notes/OPEN_QUESTIONS.md`](notes/OPEN_QUESTIONS.md)
- Current working context: [`notes/NOW.md`](notes/NOW.md)
- Book chunks 001–054 → ADR index: [`notes/study-chunks-traceability.md`](notes/study-chunks-traceability.md)
- Milestone log (post chunk spine): [`notes/DECISION_LOG.md`](notes/DECISION_LOG.md)

## Notes policy

- `notes/*` captures active thinking, open questions, and near-term planning.
- Canonical behavior and policy live in `MASTER_PLAN`, `architecture/*`, `guardrails/*`, `runbooks/*`, and `decisions/*`.
- The **`docs/`** tree is **local-only** in the public Marble repository (ignored by git). Clones from GitHub get **README + source**; maintainers who want ADRs and study notes keep a full `docs/` checkout separately. Never commit copyrighted third-party book PDFs to git.

