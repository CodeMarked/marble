# ADR critique rubric

Use this checklist when reviewing architecture decisions against open questions, implementation reality, and tests. It is meant to be **repeatable**—same lenses each time—so reviews do not drift into unstructured opinion.

## Lenses

| Lens | Questions |
| --- | --- |
| **Decision clarity** | Is there an explicit **Decision** section (not only context)? Are **alternatives considered** present when the choice was non-obvious? |
| **Scope / status honesty** | Does **Status** match reality (baseline vs final, superseded by another ADR)? Does the title or body over-claim shipped runtime behavior? |
| **Consequences** | Are trade-offs stated? Is hidden coupling called out (CI, repo layout, `game/` vs `engine/`, toolchain)? |
| **Traceability** | Per [`adr-test-traceability.md`](adr-test-traceability.md): primary code paths, **automated checks**, **Coverage status**, **Remaining gap**—do ADR claims match what tests and build actually enforce? |
| **Open-question alignment** | For the relevant row in [`notes/OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md): does this ADR **close**, **narrow**, **conflict with**, or **omit** the question? |

## Outputs of a critique pass

1. **Verdict on the open question** (not only the ADR): e.g. *Defer*, *Baseline sufficient until milestone X*, *Ready for new ADR*, *Amend existing ADR*, *Close open question* (only when an ADR fully answers it).
2. **Short bullet critique** per most relevant ADR (1–3), using the lenses above.
3. **Traceability action** (optional): one concrete **Remaining gap** line in `adr-test-traceability.md` when coverage is Partial and the topic is blocking.

## Cross-reference

- Periodic cross-walk of all open questions: [`notes/OPEN_QUESTIONS_ADR_CRITIQUE.md`](../notes/OPEN_QUESTIONS_ADR_CRITIQUE.md).
- Architecture guardrails: [`guardrails/architecture-guardrails.md`](../guardrails/architecture-guardrails.md).
