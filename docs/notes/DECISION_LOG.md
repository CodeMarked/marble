# Decision Log

Chunk **001–054** execution history is **not** duplicated here. It lives in:

- [`study-chunks-traceability.md`](study-chunks-traceability.md) — chunk → book slice → `ADR-0001`…`ADR-0054`
- [`decisions/`](../decisions/) — full ADR text
- [`MASTER_PLAN.md`](../MASTER_PLAN.md) §3 — shipped status table
- [`runbooks/adr-test-traceability.md`](../runbooks/adr-test-traceability.md) — code ↔ ADR tests

Below: **milestones after** that book-chunk spine (Vulkan, platform input, planning gates).

## 2026-03-31

- Landed first **Vulkan** presentation path (`engine/render/vulkan/VulkanRhi`) and **`marbles`** sample using `IRenderPhase`, GLSL shaders, and build-time **`glslc`** (Vulkan 1.2 SPIR-V target).
- Established **public repo vs local `docs/`** boundary: `docs/` gitignored; copyrighted book PDFs must not enter git; maintainer ADRs/notes stay in local checkouts only.
- **CI:** GitHub Actions installs Vulkan via **vcpkg** plus **`shaderc`** for `glslc`; `game/CMakeLists.txt` hints vcpkg installed-tool paths when using the vcpkg toolchain.
- Recorded fifty-fifth architecture decision in `docs/decisions/ADR-0055-vulkan-first-rhi-and-repository-docs-boundary.md`.
- Logged **follow-up** open questions: long-term shader compilation (**libshaderc** vs CLI vs offline SPIR-V), RHI thickness vs game code, optional public doc subset, dev/CI toolchain alignment (SDK vs vcpkg).
- Updated [`MASTER_PLAN.md`](../MASTER_PLAN.md), [`notes/NOW.md`](NOW.md), [`notes/OPEN_QUESTIONS.md`](OPEN_QUESTIONS.md), and [`runbooks/adr-test-traceability.md`](../runbooks/adr-test-traceability.md) to match shipped render status and backlog.

## 2026-04-01

- Extended platform **HID adapters:** [`PlatformGamepadBridge`](../../engine/input/PlatformGamepadBridge.hpp) merges the first GLFW standard gamepad into [`AbstractControl`](../../engine/input/HidMapping.hpp) (sticks, triggers, face/d-pad/shoulder/menu buttons; left stick thresholds also drive virtual D-pad for keyboard-oriented remaps). [`PlatformKeyboardBridge`](../../engine/input/PlatformKeyboardBridge.cpp) and [`Window`](../../engine/platform/window/Window.hpp) gained additional keys (Enter/Tab/shift/Ctrl/Q/E/F/digits, etc.).
- Extended **action policy** for context transitions: [`ActionContextEntry`](../../engine/input/HidMapping.hpp), [`ActionPolicy::resetActionGates`](../../engine/input/HidMapping.hpp), [`applyActionContext`](../../engine/input/HidMapping.hpp). [`MarblesGame`](../../game/marbles/MarblesGame.cpp) applies gameplay vs victory rows (disables board tilt when won; quit still polled).
- New tests: `hid_action_context_test`, `platform_gamepad_bridge_test` (GLFW init + merge smoke; finite floats with optional hardware connected).
- Updated [`MASTER_PLAN.md`](../MASTER_PLAN.md), [`notes/NOW.md`](NOW.md), [`architecture/engine-subsystems.md`](../architecture/engine-subsystems.md), [`runbooks/adr-test-traceability.md`](../runbooks/adr-test-traceability.md), [`decisions/ADR-0030-hid-context-ownership-and-input-disable-policy.md`](../decisions/ADR-0030-hid-context-ownership-and-input-disable-policy.md), root [`README.md`](../../README.md), and [`runbooks/testing.md`](../runbooks/testing.md) for backlog and traceability.
- **Open questions audit (formerly MASTER_PLAN P0 #3):** set **Owner** on the closed ADR-0054 simulation-phase row to **Marble core maintainers**; added **Current roadmap (blocking vs track)** to [`OPEN_QUESTIONS.md`](OPEN_QUESTIONS.md); recorded **done criteria** on [`MASTER_PLAN.md`](../MASTER_PLAN.md) (see §4 *Planning gates*); reordered [`NOW.md`](NOW.md) next actions around shader/RHI/ACP blocking rows vs input follow-up.
