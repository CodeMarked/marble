# System Overview

## Purpose

Marble provides a game engine runtime with clear subsystem boundaries, predictable lifecycle behavior, and practical iteration speed for development.

## Core shape

- `main` creates a single runtime owner (`core::Engine`) and delegates lifecycle to it.
- Startup (`init`) creates platform primitives first (currently windowing) and validates runtime readiness.
- Main loop (`run`) advances ordered frame phases in a deterministic single-thread baseline.
- Shutdown (`shutdown`) tears down runtime state in dependency-safe order.

## Runtime lifecycle contract (chunk 2 update)

1. `init`
   - Create platform window and set runtime flags.
   - Optionally resolve runtime **assets root** (env, override, then `<executable>/assets`) for pipeline output consumption.
   - Fail fast on platform initialization errors.
2. `run`
   - Repeat until close requested:
     - Pump input/platform events.
     - Execute simulation phase interface (`ISimulationPhase::tick`) with a fixed-step default policy.
     - Execute render phase interface (`IRenderPhase::render`).
     - Run diagnostics hook (frame health metrics/logging).
3. `shutdown`
   - Release window/platform resources.
   - Reset runtime state for clean exit.

## Design priorities

- Clarity of ownership between subsystems.
- Stable interfaces before feature expansion.
- Deterministic behavior where practical.
- Profiling-aware implementation decisions.

## Relationship to planning docs

- Priorities and phase goals: [`../MASTER_PLAN.md`](../MASTER_PLAN.md)
- Subsystem-level map: [`engine-subsystems.md`](engine-subsystems.md)
- Constraints and boundaries: [`../guardrails/architecture-guardrails.md`](../guardrails/architecture-guardrails.md)
