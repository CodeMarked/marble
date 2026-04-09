# Local Development

## Goals

- Build and run Marble quickly.
- Validate core engine behavior after changes.
- Keep local workflow consistent across contributors.

## Vulkan and shaders

- Configure needs **`glslc`** on PATH or under **`VULKAN_SDK`** (typical: install **LunarG Vulkan SDK** for Windows).
- Alternatively, use **vcpkg** with package **`shaderc`** and pass **`-DCMAKE_TOOLCHAIN_FILE=…/vcpkg.cmake`** — CMake will search `…/installed/<triplet>/tools/shaderc` automatically.
- After a `marbles` build, SPIR-V is staged as **`shaders/*.spv`** next to the executable and **`assets/shaders/*.spv`** (for loading through the runtime asset root + `BinaryResourceManager` when enabled).
- Long-term tooling (CLI vs **libshaderc** vs checked-in SPIR-V) is an open decision; see [`notes/OPEN_QUESTIONS.md`](../notes/OPEN_QUESTIONS.md) and ADR-0055 follow-ups.

## Standard flow

1. Pull latest changes.
2. Optional fast preflight (recommended):
  - `powershell -ExecutionPolicy Bypass -File .\scripts\preflight.ps1`
  - include smoke run: `powershell -ExecutionPolicy Bypass -File .\scripts\preflight.ps1 -RunHeadlessSmoke`
3. Configure and build Debug:
  - `cmake --preset debug`
  - `cmake --build --preset build-debug`
4. Run the game or smoke mode:
  - `build\vs-debug\game\Debug\marbles.exe`
  - `build\vs-debug\game\Debug\marbles.exe --headless --frames 120`
  - diagnostics cadence override: `build\vs-debug\game\Debug\marbles.exe --headless --frames 120 --diag-interval 0.5`
  - Runtime assets are staged next to the executable (`assets\`); override with `MARBLE_ASSETS_ROOT` or `marbles --assets <path>`.
5. Run tests:
  - `ctest --preset test-debug`
6. Update relevant docs when behavior or architecture changes.

## Quick checks

- Engine starts without subsystem initialization errors.
- Main loop progresses without obvious frame-stall regressions.
- Shutdown completes without leak or teardown-order warnings.

## Live development loop

- Use the built-in watcher to auto-configure, rebuild, and restart on source changes:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\dev-loop.ps1`
- The script watches `engine`, `game`, and `tests` C++/CMake files.
- On successful rebuild, it restarts `marbles.exe` automatically.

## Windows file-lock note

- If Release rebuild fails with a lock on `build\vs-release\game\Release\marbles.exe`, close any running `marbles.exe` process and retry the build.
- Typical one-liner to stop stale process:
  - `Get-Process marbles -ErrorAction SilentlyContinue | Stop-Process -Force`

