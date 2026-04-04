#include "MarblesGame.hpp"

#include "core/ResourceManager.hpp"
#include "core/Simulation.hpp"
#include "input/PlatformGamepadBridge.hpp"
#include "input/PlatformKeyboardBridge.hpp"
#include "math/Geometry.hpp"
#include "math/Mat4.hpp"
#include "math/Vec3.hpp"
#include "physics/PhysicsIntegration.hpp"
#include "platform/window/Window.hpp"
#include "render/IRenderBackend.hpp"
#include "render/RenderTypes.hpp"
#include "render/vulkan/VulkanRhi.hpp"
#include "shared/PauseMenuInput.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace marble::marbles {

namespace {

using marble::input::AbstractControl;
using marble::input::AbstractControlArray;
using marble::input::ActionContextEntry;
using marble::input::ActionPolicy;
using marble::input::ControlValueClass;
using marble::input::InputRemapTable;
using marble::input::LogicalActionId;
using marble::input::LogicalDevice;
using marble::input::actionScalar;
using marble::input::deviceMask;
using marble::input::mergeAllConnectedGamepadsIntoAbstractControls;
using marble::input::sampleKeyboardIntoAbstractControls;
using marble::math::Aabb;
using marble::math::Mat4;
using marble::math::Vec3;
using marble::physics::RigidBodyKinematics;
using marble::physics::IPhysicsWorld;
using marble::physics::SimplePhysicsWorld;
using marble::render::FrameOverlayTint;
using marble::render::IRenderBackend;
using marble::render::MeshDrawInstance;
using marble::render::VulkanRhi;

// Logical gameplay actions (keyboard mapped via `InputRemapTable` in `State`).
static constexpr LogicalActionId kActionPauseMenu = 1;
static constexpr LogicalActionId kActionBoardPitchPlus = 2;
static constexpr LogicalActionId kActionBoardPitchMinus = 3;
static constexpr LogicalActionId kActionBoardRollMinus = 4;
static constexpr LogicalActionId kActionBoardRollPlus = 5;

static constexpr ActionContextEntry kGameplayContext[] = {
    {kActionPauseMenu, deviceMask(LogicalDevice::Player), false},
    {kActionBoardPitchPlus, deviceMask(LogicalDevice::Player), false},
    {kActionBoardPitchMinus, deviceMask(LogicalDevice::Player), false},
    {kActionBoardRollMinus, deviceMask(LogicalDevice::Player), false},
    {kActionBoardRollPlus, deviceMask(LogicalDevice::Player), false},
};

static constexpr ActionContextEntry kVictoryContext[] = {
    {kActionBoardPitchPlus, deviceMask(LogicalDevice::Player), true},
    {kActionBoardPitchMinus, deviceMask(LogicalDevice::Player), true},
    {kActionBoardRollMinus, deviceMask(LogicalDevice::Player), true},
    {kActionBoardRollPlus, deviceMask(LogicalDevice::Player), true},
};

[[nodiscard]] Vec3 clampToAabb(Vec3 p, Aabb const& b) noexcept {
    return {
        std::clamp(p.x, b.min.x, b.max.x),
        std::clamp(p.y, b.min.y, b.max.y),
        std::clamp(p.z, b.min.z, b.max.z),
    };
}

/// Apply transpose of upper 3×3 rotation part of `m` to direction `d`.
[[nodiscard]] Vec3 transformDirectionTranspose(Mat4 const& m, Vec3 d) noexcept {
    return {
        m.m[0] * d.x + m.m[1] * d.y + m.m[2] * d.z,
        m.m[4] * d.x + m.m[5] * d.y + m.m[6] * d.z,
        m.m[8] * d.x + m.m[9] * d.y + m.m[10] * d.z,
    };
}

[[nodiscard]] Mat4 boardMatrix(float pitch, float roll) noexcept {
    return Mat4::rotationX(pitch) * Mat4::rotationZ(roll);
}

[[nodiscard]] Mat4 lookAtLh(Vec3 const& eye, Vec3 const& target, Vec3 const& worldUp) noexcept {
    Vec3 const f = marble::math::normalize(target - eye);
    Vec3 mutUp = worldUp;
    Vec3 r = marble::math::cross(mutUp, f);
    if (marble::math::lengthSquared(r) < 1e-8f) {
        mutUp = Vec3::unitX();
        r = marble::math::cross(mutUp, f);
    }
    r = marble::math::normalize(r);
    Vec3 const u = marble::math::cross(f, r);
    Mat4 m{};
    // Column basis [right, up, -forward]: projection uses w = -z_view so in-front geometry has z_view < 0.
    m.m[0] = r.x;
    m.m[1] = u.x;
    m.m[2] = -f.x;
    m.m[3] = 0.f;
    m.m[4] = r.y;
    m.m[5] = u.y;
    m.m[6] = -f.y;
    m.m[7] = 0.f;
    m.m[8] = r.z;
    m.m[9] = u.z;
    m.m[10] = -f.z;
    m.m[11] = 0.f;
    m.m[12] = -marble::math::dot(r, eye);
    m.m[13] = -marble::math::dot(u, eye);
    m.m[14] = marble::math::dot(f, eye);
    m.m[15] = 1.f;
    return m;
}

[[nodiscard]] Mat4 perspectiveVulkan(float fovyRad, float aspect, float n, float f) noexcept {
    float const t = std::tan(fovyRad * 0.5f);
    if (t <= 1e-8f) {
        return Mat4::identity();
    }
    float const invT = 1.f / t;
    Mat4 p{};
    p.m[0] = invT / aspect;
    p.m[5] = invT;
    p.m[10] = f / (n - f);
    p.m[11] = -1.f;
    p.m[14] = (n * f) / (n - f);
    return p;
}

void addCube(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, Vec3 color) {
    auto face = [&](Vec3 n, Vec3 t0, Vec3 t1, Vec3 t2, Vec3 t3) {
        std::uint32_t base = static_cast<std::uint32_t>(vtx.size());
        auto push = [&](Vec3 p) {
            VulkanRhi::Vertex v{};
            v.px = p.x;
            v.py = p.y;
            v.pz = p.z;
            v.nx = n.x;
            v.ny = n.y;
            v.nz = n.z;
            v.cr = color.x;
            v.cg = color.y;
            v.cb = color.z;
            vtx.push_back(v);
        };
        push(t0);
        push(t1);
        push(t2);
        push(t3);
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
        idx.push_back(base + 2);
        idx.push_back(base + 3);
        idx.push_back(base);
    };
    float const h = 0.5f;
    face({1.f, 0.f, 0.f}, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h});
    face({-1.f, 0.f, 0.f}, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h});
    face({0.f, 1.f, 0.f}, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h});
    face({0.f, -1.f, 0.f}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});
    face({0.f, 0.f, 1.f}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});
    face({0.f, 0.f, -1.f}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});
}

void addUvSphere(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    float radius,
    int stacks,
    int slices,
    Vec3 color
) {
    if (stacks < 2 || slices < 3) {
        return;
    }
    std::uint32_t base = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i <= stacks; ++i) {
        float const v = static_cast<float>(i) / static_cast<float>(stacks);
        float const phi = v * 3.14159265f;
        float const sp = std::sin(phi);
        float const cp = std::cos(phi);
        for (int j = 0; j <= slices; ++j) {
            float const u = static_cast<float>(j) / static_cast<float>(slices);
            float const theta = u * 2.f * 3.14159265f;
            float const st = std::sin(theta);
            float const ct = std::cos(theta);
            float const nx = ct * sp;
            float const ny = cp;
            float const nz = st * sp;
            VulkanRhi::Vertex vert{};
            vert.px = nx * radius;
            vert.py = ny * radius;
            vert.pz = nz * radius;
            vert.nx = nx;
            vert.ny = ny;
            vert.nz = nz;
            vert.cr = color.x;
            vert.cg = color.y;
            vert.cb = color.z;
            vtx.push_back(vert);
        }
    }
    int const stride = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int const a = base + i * stride + j;
            int const b = base + (i + 1) * stride + j;
            idx.push_back(static_cast<std::uint32_t>(a));
            idx.push_back(static_cast<std::uint32_t>(b));
            idx.push_back(static_cast<std::uint32_t>(a + 1));
            idx.push_back(static_cast<std::uint32_t>(a + 1));
            idx.push_back(static_cast<std::uint32_t>(b));
            idx.push_back(static_cast<std::uint32_t>(b + 1));
        }
    }
}

[[nodiscard]] bool resolveSphereAabb(Vec3& pos, Vec3& vel, float r, Aabb const& box, float restitution) noexcept {
    Vec3 const closest = clampToAabb(pos, box);
    Vec3 delta = pos - closest;
    float const d2 = marble::math::lengthSquared(delta);
    if (d2 >= r * r) {
        return false;
    }
    Vec3 n{};
    if (d2 < 1e-10f) {
        float const dx = std::min(pos.x - box.min.x, box.max.x - pos.x);
        float const dy = std::min(pos.y - box.min.y, box.max.y - pos.y);
        float const dz = std::min(pos.z - box.min.z, box.max.z - pos.z);
        if (dx <= dy && dx <= dz) {
            n = pos.x < (box.min.x + box.max.x) * 0.5f ? Vec3{-1.f, 0.f, 0.f} : Vec3{1.f, 0.f, 0.f};
        } else if (dy <= dz) {
            n = pos.y < (box.min.y + box.max.y) * 0.5f ? Vec3{0.f, -1.f, 0.f} : Vec3{0.f, 1.f, 0.f};
        } else {
            n = pos.z < (box.min.z + box.max.z) * 0.5f ? Vec3{0.f, 0.f, -1.f} : Vec3{0.f, 0.f, 1.f};
        }
        pos = closest + n * r;
    } else {
        float const d = std::sqrt(d2);
        n = delta * (1.f / d);
        pos = closest + n * r;
    }
    float const vn = marble::math::dot(vel, n);
    if (vn < 0.f) {
        vel = vel - n * (vn * (1.f + restitution));
    }
    return true;
}

} // namespace

struct Pickup {
    Aabb box{};
    bool taken = false;
};

namespace {

/// Board tilt + [`IPhysicsWorld::step`](physics/PhysicsIntegration.hpp) for the marble; AABB pickups and
/// custom sphere–box response remain game rules layered on the physics seam (ADR-0041).
void stepMarbleSimulation(
    float deltaSeconds,
    float boardPitch,
    float boardRoll,
    IPhysicsWorld& physics,
    RigidBodyKinematics& marble,
    Vec3 const& marbleResetPosition,
    std::vector<Aabb> const& staticColliders,
    std::vector<Pickup>& pickups,
    int& collected,
    int totalPickups,
    bool& won
) {
    Mat4 const board = boardMatrix(boardPitch, boardRoll);
    Vec3 const gWorld{0.f, -9.81f, 0.f};
    Vec3 const gLocal = transformDirectionTranspose(board, gWorld);
    auto settings = physics.settings();
    settings.gravity = gLocal;
    physics.setSettings(settings);

    physics.step(deltaSeconds, &marble, 1);

    constexpr float marbleR = 0.25f;
    constexpr float rest = 0.12f;
    for (Aabb const& box : staticColliders) {
        (void)resolveSphereAabb(marble.position, marble.linearVelocity, marbleR, box, rest);
    }

    for (Pickup& p : pickups) {
        if (p.taken) {
            continue;
        }
        Vec3 c = clampToAabb(marble.position, p.box);
        if (marble::math::lengthSquared(marble.position - c) < marbleR * marbleR) {
            p.taken = true;
            ++collected;
        }
    }

    if (marble.position.y < -2.f) {
        marble.position = marbleResetPosition;
        marble.linearVelocity = Vec3::zero();
    }
    if (collected >= totalPickups) {
        won = true;
    }
}

} // namespace

struct MarblesGame::State final {
    core::Engine& engine;
    VulkanRhi rhi;
    std::uint32_t meshCube = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t meshSphere = std::numeric_limits<std::uint32_t>::max();

    float boardPitch = 0.f;
    float boardRoll = 0.f;
    float pitchVel = 0.f;
    float rollVel = 0.f;

    RigidBodyKinematics marble{};
    SimplePhysicsWorld physics{};
    std::vector<Aabb> staticColliders{};
    std::vector<Pickup> pickups{};
    Vec3 marbleStart{};

    int totalPickups = 0;
    int collected = 0;
    bool won = false;
    double elapsed = 0.0;

    float cameraSmoothT = 0.f;

    std::vector<MeshDrawInstance> drawScratch{};

    InputRemapTable inputRemap_{};
    ActionPolicy<256> inputPolicy_{};
    AbstractControlArray controlScratch_{};
    marble::core::BinaryResourceManager<64> assetRegistry_{};
    bool assetRegistryPrimed_{false};
    bool victoryInputGatesApplied_{false};

    bool paused = false;
    bool pauseBackWasDown = false;
    bool menuQWasDown = false;
    bool menuRWasDown = false;
    bool padMenuAWasDown = false;
    bool padMenuXWasDown = false;
    bool padMenuBWasDown = false;

    explicit State(core::Engine& e) : engine(e) {
        marble.invMass = 1.f;
        marbleStart = {0.f, 0.35f, 0.f};
        marble.position = marbleStart;
        physics.setSettings({.gravity = Vec3::zero(), .maxSubSteps = 1});

        (void)inputRemap_.bind(
            AbstractControl::LPadUp,
            {kActionBoardPitchPlus, ControlValueClass::DigitalButton, false}
        );
        (void)inputRemap_.bind(
            AbstractControl::LPadDown,
            {kActionBoardPitchMinus, ControlValueClass::DigitalButton, false}
        );
        (void)inputRemap_.bind(
            AbstractControl::LPadLeft,
            {kActionBoardRollMinus, ControlValueClass::DigitalButton, false}
        );
        (void)inputRemap_.bind(
            AbstractControl::LPadRight,
            {kActionBoardRollPlus, ControlValueClass::DigitalButton, false}
        );
        (void)inputRemap_.bind(AbstractControl::BackSelect, {kActionPauseMenu, ControlValueClass::DigitalButton, false});

        inputPolicy_.resetActionGates();
        inputPolicy_.applyActionContext(std::span{kGameplayContext});

        staticColliders.push_back({{-6.f, -0.2f, -6.f}, {6.f, 0.f, 6.f}});
        float const t = 0.15f;
        float const h = 0.6f;
        staticColliders.push_back({{-6.f - t, -0.5f, -6.f}, {-6.f, h, 6.f}});
        staticColliders.push_back({{6.f, -0.5f, -6.f}, {6.f + t, h, 6.f}});
        staticColliders.push_back({{-6.f, -0.5f, -6.f - t}, {6.f, h, -6.f}});
        staticColliders.push_back({{-6.f, -0.5f, 6.f}, {6.f, h, 6.f + t}});

        pickups.push_back({{{-2.f, 0.15f, -2.f}, {-1.7f, 0.45f, -1.7f}}, false});
        pickups.push_back({{{2.f, 0.15f, 2.f}, {2.3f, 0.45f, 2.3f}}, false});
        pickups.push_back({{{-2.5f, 0.15f, 2.f}, {-2.2f, 0.45f, 2.3f}}, false});
        pickups.push_back({{{1.f, 0.15f, -2.5f}, {1.3f, 0.45f, -2.2f}}, false});
        pickups.push_back({{{0.f, 0.15f, 0.f}, {0.3f, 0.45f, 0.3f}}, false});
        totalPickups = static_cast<int>(pickups.size());
    }

    void resetMarble() {
        marble.position = marbleStart;
        marble.linearVelocity = Vec3::zero();
    }

    void restartMarblesLevel() {
        for (Pickup& p : pickups) {
            p.taken = false;
        }
        collected = 0;
        won = false;
        victoryInputGatesApplied_ = false;
        boardPitch = 0.f;
        boardRoll = 0.f;
        pitchVel = 0.f;
        rollVel = 0.f;
        resetMarble();
        inputPolicy_.resetActionGates();
        inputPolicy_.applyActionContext(std::span{kGameplayContext});
        paused = false;
        pauseBackWasDown = false;
    }

    void primeAssetRegistryOnce() {
        if (assetRegistryPrimed_) {
            return;
        }
        assetRegistryPrimed_ = true;
        std::string const root = engine.assetsRootPath();
        if (root.empty()) {
            return;
        }
        (void)marble::core::setBinaryResourceSearchRoot(assetRegistry_, std::filesystem::path(root));
        (void)assetRegistry_.acquire("README.txt");
    }

    void step(core::Engine::FrameContext const& ctx) {
        primeAssetRegistryOnce();
        if (!paused) {
            elapsed += ctx.deltaSeconds;
        }

        float const h = static_cast<float>(ctx.deltaSeconds);
        constexpr float tiltAccel = 1.8f;
        constexpr float tiltDamp = 0.92f;
        constexpr float maxTilt = 0.55f;
        constexpr LogicalDevice kPlayer = LogicalDevice::Player;

        if (auto* w = engine.window()) {
            sampleKeyboardIntoAbstractControls(*w, controlScratch_);
            mergeAllConnectedGamepadsIntoAbstractControls(controlScratch_);

            bool const pauseHeld =
                actionScalar(kActionPauseMenu, controlScratch_, inputRemap_, inputPolicy_, kPlayer) >= 0.5f ||
                w->isKeyDown(marble::platform::Key::Escape);
            if (marble::game_shared::pauseMenuToggleEdge(pauseHeld, pauseBackWasDown)) {
                paused = !paused;
            }

            if (paused) {
                bool const rDown = w->isKeyDown(marble::platform::Key::R);
                if (rDown && !menuRWasDown) {
                    restartMarblesLevel();
                }
                menuRWasDown = rDown;

                std::size_t const iPadA = static_cast<std::size_t>(AbstractControl::RPadDown);
                std::size_t const iPadX = static_cast<std::size_t>(AbstractControl::RPadLeft);
                std::size_t const iPadB = static_cast<std::size_t>(AbstractControl::RPadRight);
                bool const qDown = w->isKeyDown(marble::platform::Key::Q);
                bool const aDown = controlScratch_[iPadA] >= 0.5f;
                bool const xDown = controlScratch_[iPadX] >= 0.5f;
                bool const bDown = controlScratch_[iPadB] >= 0.5f;
                if (marble::game_shared::pauseMenuWantsReturnToLauncher(qDown, bDown, menuQWasDown, padMenuBWasDown)) {
                    engine.requestEndRun();
                }
                if (aDown && !padMenuAWasDown) {
                    paused = false;
                }
                if (xDown && !padMenuXWasDown) {
                    restartMarblesLevel();
                }
                padMenuAWasDown = aDown;
                padMenuXWasDown = xDown;
                padMenuBWasDown = bDown;
            } else {
                menuRWasDown = w->isKeyDown(marble::platform::Key::R);
                padMenuAWasDown =
                    controlScratch_[static_cast<std::size_t>(AbstractControl::RPadDown)] >= 0.5f;
                padMenuXWasDown =
                    controlScratch_[static_cast<std::size_t>(AbstractControl::RPadLeft)] >= 0.5f;
                marble::game_shared::syncPauseMenuReturnEdgeState(
                    w->isKeyDown(marble::platform::Key::Q),
                    controlScratch_[static_cast<std::size_t>(AbstractControl::RPadRight)] >= 0.5f,
                    menuQWasDown,
                    padMenuBWasDown);

                if (!won) {
                    float const pitchPlus =
                        actionScalar(kActionBoardPitchPlus, controlScratch_, inputRemap_, inputPolicy_, kPlayer);
                    float const pitchMinus =
                        actionScalar(kActionBoardPitchMinus, controlScratch_, inputRemap_, inputPolicy_, kPlayer);
                    float const rollMinus =
                        actionScalar(kActionBoardRollMinus, controlScratch_, inputRemap_, inputPolicy_, kPlayer);
                    float const rollPlus =
                        actionScalar(kActionBoardRollPlus, controlScratch_, inputRemap_, inputPolicy_, kPlayer);
                    float const pIn = pitchPlus - pitchMinus;
                    float const rIn = rollPlus - rollMinus;
                    pitchVel += pIn * tiltAccel * h;
                    rollVel += rIn * tiltAccel * h;
                }
            }
        }

        if (!paused && !won) {
            pitchVel *= tiltDamp;
            rollVel *= tiltDamp;
            boardPitch += pitchVel * h;
            boardRoll += rollVel * h;
            boardPitch = std::clamp(boardPitch, -maxTilt, maxTilt);
            boardRoll = std::clamp(boardRoll, -maxTilt, maxTilt);

            stepMarbleSimulation(
                h,
                boardPitch,
                boardRoll,
                physics,
                marble,
                marbleStart,
                staticColliders,
                pickups,
                collected,
                totalPickups,
                won
            );
        }

        if (won && !victoryInputGatesApplied_) {
            inputPolicy_.resetActionGates();
            inputPolicy_.applyActionContext(std::span{kVictoryContext});
            victoryInputGatesApplied_ = true;
            pitchVel = 0.f;
            rollVel = 0.f;
        }

        if (auto* w = engine.window()) {
            char buf[160];
            if (paused) {
                (void)std::snprintf(buf, sizeof(buf), "Marbles — Paused — Esc resume · R restart · Q menu");
            } else if (won) {
                (void)std::snprintf(
                    buf,
                    sizeof(buf),
                    "Marbles — Won %.1fs — Esc pause · Q menu when paused",
                    static_cast<float>(elapsed)
                );
            } else {
                (void)std::snprintf(
                    buf,
                    sizeof(buf),
                    "Marbles — Gems %d/%d — Esc pause",
                    collected,
                    totalPickups
                );
            }
            w->setTitle(buf);
        }
    }

    void renderFrame(core::Engine::FrameContext const& ctx) {
        (void)ctx;
        IRenderBackend& backend = rhi;
        if (!backend.initialized() || meshCube == std::numeric_limits<std::uint32_t>::max() ||
            meshSphere == std::numeric_limits<std::uint32_t>::max()) {
            return;
        }

        int fbW = 1, fbH = 1;
        if (auto* w = engine.window()) {
            w->getFramebufferSize(&fbW, &fbH);
        }
        float const aspect = static_cast<float>(fbW) / static_cast<float>(std::max(1, fbH));

        Mat4 const board = boardMatrix(boardPitch, boardRoll);
        Vec3 const marbleWorld = marble::math::transformPoint(board, marble.position);

        Vec3 const camTarget = marbleWorld;
        Vec3 camPos{marbleWorld.x - 5.f, marbleWorld.y + 4.f, marbleWorld.z - 5.f};
        cameraSmoothT = std::min(1.f, cameraSmoothT + 0.05f);
        Mat4 const view = lookAtLh(camPos, camTarget, Vec3::unitY());
        Mat4 const proj = perspectiveVulkan(60.f * 3.14159265f / 180.f, aspect, 0.1f, 80.f);
        Mat4 const viewProj = proj * view;

        drawScratch.clear();
        drawScratch.reserve(32);

        auto pushCube = [&](Mat4 const& model, Vec3 color) {
            MeshDrawInstance d{};
            d.meshIndex = meshCube;
            d.model = model;
            d.color = color;
            drawScratch.push_back(d);
        };

        constexpr float tWall = 0.15f;
        constexpr float hWall = 0.6f;

        pushCube(board * Mat4::translation({0.f, -0.1f, 0.f}) * Mat4::scaling({12.f, 0.2f, 12.f}), {0.35f, 0.42f, 0.5f});

        pushCube(
            board * Mat4::translation({-6.f - tWall * 0.5f, (hWall - 0.5f) * 0.5f, 0.f}) *
                Mat4::scaling({tWall, hWall + 0.5f, 12.f}),
            {0.5f, 0.35f, 0.3f}
        );
        pushCube(
            board * Mat4::translation({6.f + tWall * 0.5f, (hWall - 0.5f) * 0.5f, 0.f}) *
                Mat4::scaling({tWall, hWall + 0.5f, 12.f}),
            {0.5f, 0.35f, 0.3f}
        );
        pushCube(
            board * Mat4::translation({0.f, (hWall - 0.5f) * 0.5f, -6.f - tWall * 0.5f}) *
                Mat4::scaling({12.f, hWall + 0.5f, tWall}),
            {0.5f, 0.35f, 0.3f}
        );
        pushCube(
            board * Mat4::translation({0.f, (hWall - 0.5f) * 0.5f, 6.f + tWall * 0.5f}) *
                Mat4::scaling({12.f, hWall + 0.5f, tWall}),
            {0.5f, 0.35f, 0.3f}
        );

        for (Pickup const& p : pickups) {
            if (p.taken) {
                continue;
            }
            Vec3 const ext = (p.box.max - p.box.min) * 0.5f;
            Vec3 const ctr = (p.box.min + p.box.max) * 0.5f;
            pushCube(board * Mat4::translation(ctr) * Mat4::scaling(ext * 2.f), {0.95f, 0.85f, 0.2f});
        }

        {
            MeshDrawInstance d{};
            d.meshIndex = meshSphere;
            d.model = board * Mat4::translation(marble.position) * Mat4::scaling({0.25f, 0.25f, 0.25f});
            d.color = {0.85f, 0.2f, 0.15f};
            drawScratch.push_back(d);
        }

        if (auto* w = engine.window()) {
            FrameOverlayTint tint{};
            FrameOverlayTint const* overlay = nullptr;
            if (paused) {
                tint = FrameOverlayTint{0.02f, 0.025f, 0.07f, 0.42f};
                overlay = &tint;
            }
            (void)backend.drawFrame(*w, viewProj, drawScratch, overlay);
        }
    }
};

class RenderPhase final : public core::Engine::IRenderPhase {
public:
    explicit RenderPhase(MarblesGame::State* s) : state_(s) {}

    void render(core::Engine::FrameContext const& ctx) override {
        if (state_) {
            state_->renderFrame(ctx);
        }
    }

private:
    MarblesGame::State* state_;
};

MarblesGame::MarblesGame(core::Engine& engine) : engine_(engine), state_(std::make_unique<MarblesGame::State>(engine)) {}

MarblesGame::~MarblesGame() {
    if (state_) {
        state_->rhi.shutdown();
    }
    engine_.resetPhasesToDefaults();
}

void MarblesGame::installPhases() {
    auto* raw = state_.get();
    auto rend = std::make_unique<RenderPhase>(raw);
    auto fixed = std::make_unique<core::FixedStepSimulationPhase>();
    fixed->setStepCallback([raw](core::Engine::FrameContext const& ctx) { raw->step(ctx); });
    engine_.setSimulationPhase(std::move(fixed));
    engine_.setRenderPhase(std::move(rend));
}

bool MarblesGame::initGraphics(std::string shaderDirectory, std::optional<std::uint32_t> physicalDeviceIndex) {
    if (!engine_.window()) {
        return false;
    }
    bool vkOk = false;
    std::string const assetsRoot = engine_.assetsRootPath();
    if (!assetsRoot.empty()) {
        (void)marble::core::setBinaryResourceSearchRoot(state_->assetRegistry_, std::filesystem::path(assetsRoot));
        // ResourceKind::ShaderBytecode — registry path matches staged `assets/shaders/*.spv`.
        if (state_->assetRegistry_.acquire("shaders/mesh.vert.spv") &&
            state_->assetRegistry_.acquire("shaders/mesh.frag.spv")) {
            marble::core::BinaryResource const* const vertRes = state_->assetRegistry_.find("shaders/mesh.vert.spv");
            marble::core::BinaryResource const* const fragRes = state_->assetRegistry_.find("shaders/mesh.frag.spv");
            if (vertRes != nullptr && fragRes != nullptr) {
                std::span<std::uint8_t const> const vspan(vertRes->bytes.data(), vertRes->bytes.size());
                std::span<std::uint8_t const> const fspan(fragRes->bytes.data(), fragRes->bytes.size());
                vkOk = state_->rhi.initFromSpirvBytes(
                    *engine_.window(), "Marbles", vspan, fspan, physicalDeviceIndex);
            }
        }
    }
    if (!vkOk) {
        if (!state_->rhi.init(*engine_.window(), "Marbles", std::move(shaderDirectory), physicalDeviceIndex)) {
            return false;
        }
    }
    IRenderBackend& renderBackend = state_->rhi;
    renderBackend.setClearColor(0.06f, 0.07f, 0.1f, 1.f);

    std::vector<VulkanRhi::Vertex> cv;
    std::vector<std::uint32_t> ci;
    addCube(cv, ci, {1.f, 1.f, 1.f});
    state_->meshCube = state_->rhi.uploadMesh(cv, ci);
    if (state_->meshCube == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<VulkanRhi::Vertex> sv;
    std::vector<std::uint32_t> si;
    addUvSphere(sv, si, 1.f, 16, 24, {1.f, 1.f, 1.f});
    state_->meshSphere = state_->rhi.uploadMesh(sv, si);
    if (state_->meshSphere == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    return true;
}

} // namespace marble::marbles
