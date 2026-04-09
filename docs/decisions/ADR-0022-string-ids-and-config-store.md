# ADR-0022: String IDs and configuration store baseline (Chapter §6.4 -> §6.5)

## Status

Accepted

## Context

Chunk 022 covers string/runtime costs, hashed string identifiers, localization considerations, and configuration loading mechanisms (text files, command line, environment, per-user options).

Marble needs a small foundational layer for:
- fast identifier comparison without raw `strcmp`-heavy call sites, and
- unified option ingestion paths for development/runtime knobs.

## Decision

1. Add [`core::StringId`](../../engine/core/StringId.hpp):
   - 64-bit hashed IDs (`fnv1a64`) for fast integer comparisons.
   - constexpr `makeStringId()` and compile-time user-defined literal (`"_sid"`).
2. Add [`core::ConfigStore`](../../engine/core/ConfigStore.hpp):
   - key/value option storage keyed by `StringId`,
   - ingestion from simple key/value text (`key = value`),
   - command-line overrides via `--key=value`,
   - typed reads (`getBool/getInt/getFloat/getString`),
   - option flags for `persistent` and `perUser`.
3. Keep scope intentionally minimal:
   - no localization database/runtime lookup API yet,
   - no file I/O backend in this chunk (parsing/storage only),
   - no collision-recovery string table for `StringId` reverse lookup yet.

## Consequences

- Positive: establishes a clear, testable baseline for string IDs and config plumbing.
- Positive: supports practical configuration layering (defaults + text + command line).
- Trade-off: hashed IDs can collide in theory; reverse lookup and collision diagnostics are deferred.

## Alternatives considered

- **Raw string keys everywhere:** rejected for hot-path identifier comparisons and long-term consistency.
- **Full localization/config system now:** deferred to keep this chunk focused on foundational runtime primitives.
