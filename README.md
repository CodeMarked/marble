# Marble

C++20 game project with a **Vulkan** rendering path and a small **runtime engine** (windowing, frame phases, logging, asset-root resolution). Much of what lives under `engine/` is **architecture and policy**—headers and tests that describe future systems (physics, audio, animation, networking, and similar)—and is **not** all wired into the shipped `marbles` demo yet.

## Build (recommended: CMake presets)

From the project root in **PowerShell** (Visual Studio 2022 + CMake required):

**Debug**

```powershell
cmake --preset debug
cmake --build --preset build-debug
```

**Release**

```powershell
cmake --preset release
cmake --build --preset build-release
```

Outputs go under `build\vs-debug\` and `build\vs-release\` (multi-config Visual Studio generator).

### Compiler policy (Marble targets)

Engine, game, and tests inherit shared flags from [`cmake/MarbleCompileOptions.cmake`](cmake/MarbleCompileOptions.cmake): MSVC uses `/W4` and `/permissive-`; other toolchains use `-Wall -Wextra -Wpedantic`. **Debug** defines `MARBLE_DEBUG=1` for conditional compilation. Third-party deps (for example GLFW) are not forced to match. Details: [`docs/decisions/ADR-0006-toolchain-compile-policy.md`](docs/decisions/ADR-0006-toolchain-compile-policy.md).

## Run

- **Debug:** `build\vs-debug\game\Debug\marbles.exe`
- **Release:** `build\vs-release\game\Release\marbles.exe`

Optional: run without a window for a short time (good for quick checks):

```powershell
build\vs-debug\game\Debug\marbles.exe --headless --frames 120
```

Adjust diagnostics print cadence (seconds), or disable diagnostics entirely (`<= 0`):

```powershell
build\vs-debug\game\Debug\marbles.exe --headless --frames 120 --diag-interval 0.5
```

On a machine with multiple Vulkan adapters, you can select the **n**th suitable device after the usual sorting (discrete GPUs are preferred before integrated):

```powershell
build\vs-debug\game\Debug\marbles.exe --gpu 0
```

## Test

After a successful configure + build for that preset:

```powershell
ctest --preset test-debug
# or
ctest --preset test-release
```

CTest runs these targets (see [`docs/runbooks/testing.md`](docs/runbooks/testing.md) for intent):

| Test | What it checks |
| --- | --- |
| `engine_smoke_test` | Headless engine init → bounded frames → shutdown |
| `fixed_step_simulation_test` | Fixed-step accumulator and cap behavior |
| `asset_root_resolution_test` | `resolveAssetsRoot` success and failure paths |

You can also configure with a plain out-of-tree build (e.g. `cmake -B build -S .`) and run `ctest -C Debug` from `build/` if you are not using presets.

## Live rebuild loop

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\dev-loop.ps1
```

Watches source, rebuilds, and restarts `marbles.exe`. If Release rebuild fails because `marbles.exe` is locked, close the app or: `Get-Process marbles -ErrorAction SilentlyContinue | Stop-Process -Force`

## Preflight workflow

Run these from the **repo root** (or anywhere—the scripts `cd` to the root first). Preflight runs, in order: `git status --short`, `cmake --preset`, `cmake --build`, `ctest`. It does **not** call GitHub; any “github” highlights in the terminal are usually link styling on normal text.

Standard local preflight (status, configure, build, test):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\preflight.ps1
```

If you double-click the script and the window closes too fast, add `-PauseAtEnd` (or run from an open PowerShell window so output stays).

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\preflight.ps1 -PauseAtEnd
```

Include headless smoke run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\preflight.ps1 -RunHeadlessSmoke
```

---

If `cmake` is not recognized, add CMake’s `bin` to your PATH.
