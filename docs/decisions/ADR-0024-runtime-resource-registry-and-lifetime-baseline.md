# ADR-0024: Runtime resource registry and lifetime baseline (Chapter §7.2)

## Status

Accepted

## Context

Chunk 024 covers `§7.2 The Resource Manager`, with emphasis on runtime responsibilities:
- keep only one copy of each resource in memory,
- maintain a resource registry keyed by GUID,
- manage resource lifetimes (often via reference counting),
- defer/load policies and streaming complexity.

Marble already has filesystem wrappers from chunk 023; it now needs a minimal runtime resource manager baseline.

## Decision

1. Add [`core::BinaryResourceManager`](../../engine/core/ResourceManager.hpp):
   - virtual-path GUID via `StringId`,
   - in-memory registry of loaded binary resources,
   - synchronous acquire/load and release/unload API.
2. Enforce **single-copy** semantics:
   - repeated acquire of the same virtual path increments `refCount` instead of duplicating memory.
3. Manage lifetime by **reference counting**:
   - `acquire()` increments,
   - `release()` decrements and unloads when count reaches zero.
4. Resolve virtual paths against configured search roots using chunk-023 filesystem API.
5. Defer asynchronous streaming, composite dependency graph traversal/fix-up, and GPU upload/log-in stages.

## Consequences

- Positive: concrete runtime registry now exists and is test-covered.
- Positive: clear lifetime policy and predictable one-copy behavior for binary resources.
- Trade-off: this baseline supports only simple binary blobs and synchronous loads.

## Alternatives considered

- **Auto-load any missing resource from anywhere at use-site:** rejected as default policy due to hitch risk and lack of explicit lifetime control.
- **Implement full composite resource graph + async streaming now:** deferred to keep chunk 024 scope narrow and verifiable.
