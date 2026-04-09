# ADR-0012: OS boundary and process identity

## Status

Accepted

## Context

Chapter 4.4 covers operating-system roles: processes, threads, virtual memory, scheduling, and the boundary between user mode and the kernel. Marble already isolates some OS concerns under [`platform/`](../../engine/platform/) (paths, windowing, hardware hints in [ADR-0009](ADR-0009-hardware-query-surface.md)) and keeps the main loop single-threaded ([ADR-0001](ADR-0001-deterministic-single-thread-runtime-loop.md), [ADR-0011](ADR-0011-baseline-single-thread-and-explicit-parallelism-hints.md)). We still need an explicit rule for **where OS-specific calls live** and a minimal **process identity** hook for future logging and diagnostics.

## Decision

1. **OS-facing APIs** that are not pure C++ standard library belong under **`marble::platform`** with narrow headers (for example [`platform/os/ProcessId.hpp`](../../engine/platform/os/ProcessId.hpp)), not scattered through `core::*` translation units.
2. Expose **`currentProcessId()`** returning a **`std::uint32_t`**, implemented with **`GetCurrentProcessId`** on Windows and **`getpid`** on POSIX. This is for **identity and correlation**, not security or sandboxing.
3. **Time** for frame scheduling and simulation remains **`std::chrono`** in engine core unless a platform monotonic clock with different guarantees is required later.
4. **No “OS abstraction layer”** that virtualizes every syscall: add small wrappers only when a second call site needs them or tests require seams.

## Consequences

- Positive: one documented place for “where did OS calls go?” when reviewing code.
- Positive: diagnostics can eventually print PID without each subsystem including platform headers ad hoc.
- Trade-off: `std::uint32_t` may theoretically truncate exotic PID spaces; acceptable for current targets; widen if a port requires it.

## Alternatives considered

- **Use only `std::this_thread` / no PID:** rejected — process identity is a standard OS concept and useful for multi-instance debugging.
- **Central `Os` facade class:** deferred until more than a handful of OS entry points accumulate.
