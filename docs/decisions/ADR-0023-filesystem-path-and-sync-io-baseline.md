# ADR-0023: File system path and synchronous I/O baseline (Chapter 7 -> §7.1)

## Status

Accepted

## Context

Chunk 023 covers Chapter 7 intro and `§7.1` file system concerns:
- cross-platform path differences and path APIs,
- basic file I/O wrappers,
- search paths,
- and asynchronous I/O concepts.

Marble needs a small cross-platform file system surface before the resource manager work in later chunks.

## Decision

1. Add path helpers in [`platform/filesystem/Path.hpp`](../../engine/platform/filesystem/Path.hpp):
   - `normalize`, `isAbsolute`, `join`,
   - `directoryOf`, `filenameOf`, `extensionOf`,
   - `splitSearchPath` with platform delimiter.
2. Add synchronous file I/O helpers in [`platform/filesystem/FileSystem.hpp`](../../engine/platform/filesystem/FileSystem.hpp):
   - `exists`,
   - `readBinaryFile`,
   - `readTextFile`,
   - `findOnSearchPath`.
3. Keep this API wrapper lightweight and portable, implemented via C++ standard library primitives.
4. Defer asynchronous I/O scheduler/threading API and media-specific backends (DVD/Blu-ray/network) to later chunks.

## Consequences

- Positive: a consistent engine-owned path/file API now exists across platforms.
- Positive: enables upcoming resource loading code to avoid direct dependency on platform-specific calls.
- Trade-off: current I/O is synchronous only; streaming/deadline-aware request handling is not yet present.

## Alternatives considered

- **Use raw OS APIs directly everywhere:** rejected due to portability and maintainability concerns.
- **Implement async streaming immediately:** deferred to keep scope aligned with chunk 023.
