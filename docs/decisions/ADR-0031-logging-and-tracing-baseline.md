# ADR-0031: Logging and tracing baseline (Chapter 10 §10.1)

## Status

Accepted

## Context

Chapter 10 introduces development-time diagnostics: formatted log lines, verbosity thresholds, and channel-based routing so teams can narrow output without recompiling. The engine already used ad hoc `std::cout` in places; a single policy avoids scattering formats and makes tests able to assert routing without relying on console capture.

## Decision

1. Add `[core/Log.hpp](../../engine/core/Log.hpp)` and `[core/Log.cpp](../../engine/core/Log.cpp)` with:
   - `logPrintf` / `vlogPrintf` using a fixed-size `vsnprintf` buffer (book-style),
   - global verbosity: a line prints only if `messageVerbosity <= globalLogVerbosity()`,
   - global channel mask: a line prints only if its channel bits overlap the filter, **except** `channels == 0` skips the overlap check (treated as untagged / always pass the channel stage),
   - injectable `setLogSink` for unit tests and future file/network mirrors,
   - default sink: `stderr` plus `OutputDebugStringA` on Windows.
2. Define a small `LogChannel` enum (`General`, `Engine`, `Render`, `Input`) as the initial routing vocabulary; extend with more bits as subsystems grow.
3. Route first-party runtime messages through `logPrintf` instead of ad hoc `std::cout`/`std::cerr`: `[core/Engine.cpp](../../engine/core/Engine.cpp)`, `[core/AssetRoot.cpp](../../engine/core/AssetRoot.cpp)`, and `[game/marbles/main.cpp](../../game/marbles/main.cpp)` (game banner, CLI parse errors, init failure). Operational lines use verbosity `0` and `LogChannel::Engine` (core) or `LogChannel::General` (executable entrypoint).

## Consequences

- Positive: one place to tighten format, flush policy, and platform hooks.
- Positive: tests can route logs through an in-memory sink without parsing stderr.
- Trade-off: no thread-safe queue or async logging yet; acceptable for single-threaded frame loop and policy tests.
- Note: user-visible stream changed from stdout to stderr for messages that previously used `std::cout`; this matches typical logging conventions and the default sink.
- Follow-up: optional file mirror, ring buffer for crash logs, and async/thread-safe drains.

## Alternatives considered

- **Header-only + iostream:** rejected to keep Windows debug output and sink injection in one `.cpp` without pulling `windows.h` into widespread includes.
- **Third-party logging library:** deferred to keep the chunk aligned with the book’s minimal API surface.
