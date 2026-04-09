# ADR-0004: Runtime assets root resolution and staging

## Status

Accepted

## Context

Section 1.7 describes digital content creation tools, the asset conditioning pipeline, and the flow of data into the runtime engine. Marble had no explicit runtime location for shipped assets, which blocks a future resource manager and confuses local versus installed layouts.

## Decision

1. Define a **runtime assets root** as a single directory the engine resolves at startup when enabled.
2. Resolution order:
   - `MARBLE_ASSETS_ROOT` environment variable when it points to an existing directory.
   - Optional string override from application config (CLI: `marbles --assets <path>`).
   - `<executable_directory>/assets` when the executable directory is known (Windows implementation first).
3. CMake copies the repository `assets/` tree to `$<TARGET_FILE_DIR:marbles>/assets` on each `marbles` build so the default path works out of the box.
4. `requireAssetsDirectory` fails `Engine::init` when true and no valid root is found; default remains non-fatal for incremental bring-up.

## Consequences

- Positive: clear handoff from build/pipeline output to runtime consumption.
- Positive: supports self-host, central server, and streaming clients that mount content differently.
- Trade-off: non-Windows targets currently lack executable-directory detection; overrides or env remain required there until extended.
- Follow-up: implement executable directory on Linux/macOS and add a proper resource manager interface.

## Alternatives considered

- Hard-code cwd-relative `assets/` only: rejected (fragile under IDEs, services, and servers).
- Embed all assets in the binary: rejected (poor iteration and scale for a real engine).
