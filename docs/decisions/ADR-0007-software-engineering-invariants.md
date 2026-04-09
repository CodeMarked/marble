# ADR-0007: Software engineering invariants (assertions and unreachable paths)

## Status

Accepted

## Context

Chapter 3 (“Fundamentals of Software Engineering for Games”) covers coding standards, defensive practices, and error-handling philosophy. Marble already defines `MARBLE_DEBUG` for Debug configurations ([ADR-0006](ADR-0006-toolchain-compile-policy.md)) but had no shared, documented pattern for **programmer invariants** versus **recoverable runtime failures** (for example `init()` returning `false`).

## Decision

1. **Programmer invariants** (logic that must hold if the code is correct) use **`MARBLE_ASSERT(expr)`**, implemented via `<cassert>` `assert` when `MARBLE_DEBUG` is enabled, and compiled out otherwise.
2. **Unreachable control flow** after exhaustive `switch` coverage (or similar) uses **`MARBLE_UNREACHABLE()`**, implemented with `__assume(0)` on MSVC, `__builtin_unreachable()` on GCC/Clang, and `std::abort()` elsewhere.
3. **Recoverable failures** remain explicit API outcomes (`bool`, error codes, optional values)—not assertions—for paths that depend on user data, I/O, or platform behavior.
4. Frame orchestration invariants in `core::Engine` (simulation and render phases present during `runFrame`) are asserted with `MARBLE_ASSERT` to catch wiring regressions early in Debug builds.

## Consequences

- Positive: one place to explain assert vs recoverable failure for new engine code.
- Positive: Debug builds catch null phase pointers immediately if lifecycle rules change.
- Trade-off: assertions are not a substitute for validation of external input; public surfaces must still report failure explicitly.
- Follow-up: if `VERIFY`-style “evaluate always, assert in Debug” macros are needed for third-party call results, add them with a `MARBLE_` prefix and document Windows macro hygiene.

## Alternatives considered

- **Rely on `assert`/`NDEBUG` only:** rejected for portable engine code; `MARBLE_DEBUG` is the project’s explicit hook ([ADR-0006](ADR-0006-toolchain-compile-policy.md)).
- **Custom assert handler with logging:** deferred until a logging subsystem exists; `assert` is sufficient for the current milestone.
