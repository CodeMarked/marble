# Testing

## Expectations

- Changes to engine behavior include verification steps.
- Core subsystem changes include focused tests where practical.
- Regressions are prevented with repeatable checks.

## Test strategy

- Fast local checks for compile and basic runtime health.
- Targeted tests for touched subsystems and interfaces.
- Broader integration checks before merging significant architecture work.

## Current automated checks

- Configure/build in **Debug** and **Release** via CMake presets (`cmake --preset debug|release`, `cmake --build --preset build-debug|build-release`).
- Run all registered tests:
  - `ctest --preset test-debug`
  - `ctest --preset test-release`
- CI (`.github/workflows/build.yml`) mirrors the same preset configure, build, and test flow on `windows-latest` with vcpkg Vulkan toolchain.

## Test inventory (CTest)

The authoritative list is [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) (`add_executable` + `add_test`). Below is a **partial** index for navigation; run `ctest -N` after configure to enumerate everything.

| CTest name | Executable | Primary subsystem / contract |
| --- | --- | --- |
| `engine_smoke_test` | `engine_smoke_test` | Runtime lifecycle: headless init, bounded `run`, shutdown |
| `fixed_step_simulation_test` | `fixed_step_simulation_test` | `FixedStepSimulationPhase`: step count and overshoot / cap behavior |
| `asset_root_resolution_test` | `asset_root_resolution_test` | `resolveAssetsRoot`: valid override path vs missing path with `requireExistingDirectory` |
| `hid_input_policy_test` | `hid_input_policy_test` | `input/Hid.hpp` dead zones, filtering, button edge extraction |
| `hid_gesture_policy_test` | `hid_gesture_policy_test` | `input/Hid.hpp` chord/tap/sequence gestures |
| `hid_mapping_policy_test` | `hid_mapping_policy_test` | `input/HidMapping.hpp` remap + `ControllerRouter` |
| `hid_action_policy_test` | `hid_action_policy_test` | `ActionPolicy` owner masks and per-action disable |
| `hid_action_context_test` | `hid_action_context_test` | `applyActionContext` / `resetActionGates` / `clearAllDisabled` interaction |
| `platform_keyboard_bridge_test` | `platform_keyboard_bridge_test` | `actionScalar` + `InputRemapTable` + `ActionPolicy` wiring |
| `platform_gamepad_bridge_test` | `platform_gamepad_bridge_test` | GLFW init + `mergeFirstGamepadIntoAbstractControls` smoke |

Implementation lives under [`tests/`](../../tests/). Adding a new test: register the executable in [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) and add `add_test(NAME … COMMAND …)`.

Decision mapping reference: [`adr-test-traceability.md`](adr-test-traceability.md).

## Plain `build/` directory (no presets)

If you use `cmake -B build -S .` from the repo root:

- Build: `cmake --build build --config Debug` (or `Release`).
- Test: `ctest -C Debug --output-on-failure` from `build/` (multi-config generators).

## Minimum verification in change notes

- What was tested.
- How it was tested.
- What result was observed.
