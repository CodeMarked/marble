# ADR-0050: World chunk data, object references, and queries baseline (Chapter 16 §16.3–§16.5)

## Status

Accepted

## Context

[`docs/book/chunks/index.md`](../book/chunks/index.md) places **chunk 050** in **§16.3 World Chunk Data Formats** through **§16.5 Object References and World Queries**. After introducing a runtime object model in chunk 049, Marble needs a world-data seam for chunk-addressed records, stable object references, and simple query surfaces.

## Decision

1. Add [`gameplay/WorldDataFormats.hpp`](../../engine/gameplay/WorldDataFormats.hpp):
   - `ChunkCoord` and `packChunkCoordKey` for deterministic chunk-key packing.
   - `WorldObjectRef` (`guid`) and validity helper.
   - `ChunkObjectRecord` (`self`, `archetypeId`, `position`, optional `parent` reference).
   - `WorldChunkData<MaxRecords>` with bounded append and GUID lookup helpers.
   - `queryRecordsInAabb` for coarse world queries against chunk records.
2. Verify with `world_data_formats_test`.

## Consequences

- Positive: gameplay/runtime can reason about chunked record data and references before streaming/persistence backends.
- Positive: object reference and query helpers stay deterministic and testable.
- Trade-off: no binary serialization schema, no multi-chunk query acceleration structure.
- Follow-up: chunk 051+ for update scheduling/concurrency and broader world systems.

## Alternatives considered

- **Direct runtime-object pointers inside chunk records:** rejected; GUID references remain stable across load/unload boundaries.
- **Immediate spatial index integration:** deferred; start with bounded per-chunk query helpers.
