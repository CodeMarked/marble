# ADR-0005: Preset-driven preflight workflow and diagnostics interval knob

## Status

Accepted

## Context

Chapter 2 emphasizes reliable engineering tools, source-control-safe workflows, and practical day-to-day iteration discipline. Marble already had CMake presets and tests, but verification remained spread across multiple ad hoc command sequences.

## Decision

1. Standardize local verification through `scripts/preflight.ps1`:
   - show git status snapshot,
   - configure via preset,
   - build via preset,
   - run tests via preset,
   - optional headless smoke run.
2. Add `diagnosticsIntervalSeconds` to `Engine::Config` and expose it as `marbles --diag-interval <seconds>`:
   - default `1.0`,
   - values `<= 0` disable diagnostics output.

## Consequences

- Positive: reproducible local validation path aligned with team workflows and CI expectations.
- Positive: lower-friction profiling/debug iteration without code edits.
- Trade-off: one more script and runtime option to maintain.

## Alternatives considered

- Keep only README command snippets: rejected as too easy to drift and omit steps.
- Hard-code diagnostics cadence: rejected due to poor flexibility for testing/profiling.

## Follow-up actions

- Add equivalent preflight workflow to CI documentation and PR templates.
- Add finer-grained diagnostics counters (simulation substeps, dropped batches) to output.
- Consider non-PowerShell preflight parity for non-Windows host development.
