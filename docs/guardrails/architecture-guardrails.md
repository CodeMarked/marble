# Architecture Guardrails

## Boundary rules

- Subsystems communicate through defined interfaces, not internal implementation details.
- Lifecycle order is explicit and consistent: init -> run -> shutdown.
- Keep dependency direction intentional; avoid circular coupling between major subsystems.

## Implementation rules

- Prefer small, testable changes that preserve runtime stability.
- Introduce concurrency only with clear ownership and synchronization strategy.
- Require measurable profiling evidence for performance-motivated complexity.

## Decision rules

- Record major architecture choices in `docs/decisions/*`.
- Update related architecture and runbook docs when behavior changes.
- Resolve open questions with an **owner** before large expansions. Prefer **target decision gates tied to book chunks** (see [`notes/OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md)) instead of calendar dates unless a ship milestone forces an earlier cut.

## External C++ integrators

- Prefer **narrow, documented interfaces** at subsystem boundaries; keep implementation detail private to modules where possible.
- **Document breaking changes** when behavior or CMake targets that integrators rely on change; align with the **public API / versioning** decision in [`notes/OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) when it lands.
- Add or extend **tests** for behaviors and contracts promised to external consumers (same spirit as ADR–test traceability runbooks).
- Distinguish **experimental** APIs from **supported** SDK surface once versioning policy exists.
