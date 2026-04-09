# ADR-0006: Marble toolchain compile and preprocessor policy

## Status

Accepted

## Context

Section 2.2 describes how compilers and linkers transform translation units, how build configurations bundle flags, and how debug builds expose symbols like `_DEBUG` for conditional compilation. Marble previously relied on CMake defaults without an explicit, documented policy for Marble-owned targets.

## Decision

1. Introduce a CMake `INTERFACE` library target `marble_compile_options` defined in [`cmake/MarbleCompileOptions.cmake`](../../cmake/MarbleCompileOptions.cmake).
2. Apply the following defaults to Marble code (via `PUBLIC` linking from `engine`, so `game` and `tests` inherit):
   - **MSVC:** `/W4`, `/permissive-`
   - **GCC/Clang:** `-Wall`, `-Wextra`, `-Wpedantic`
3. Define `MARBLE_DEBUG=1` for multi-config `Debug` builds using generator expressions (Visual Studio style presets).
4. Do **not** attach these flags to third-party targets (for example GLFW via FetchContent); they keep upstream settings.

## Consequences

- Positive: consistent warning and conformance discipline across Marble translation units.
- Positive: explicit hook for future conditional compilation (`#ifdef MARBLE_DEBUG`) without abusing `_DEBUG` directly in portable code.
- Trade-off: stricter MSVC settings may surface new warnings; they should be fixed or addressed with narrowly scoped pragmas, not silenced globally.
- Follow-up: consider `/WX` or `-Werror` on CI once warning-clean builds are stable on all platforms.

## Alternatives considered

- Global `add_compile_options` for the whole project: rejected because it would force third-party code to match Marble’s policy.
- Rely on IDE-only settings: rejected because CI and headless builds would drift from local developer machines.
