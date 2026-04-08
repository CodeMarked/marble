#include "MarbleApp.hpp"

#include "garden/GardenGame.hpp"
#include "MarblesGame.hpp"

#include "core/ResourceManager.hpp"
#include "math/Mat4.hpp"
#include "platform/window/Window.hpp"
#include "render/DrawFlags.hpp"
#include "render/IRenderBackend.hpp"
#include "render/vulkan/VulkanRhi.hpp"
#include "ui/EasyFontMesh.hpp"
#include "ui/NdcRect.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace marble::marbles_app {

using marble::core::Engine;

namespace {

using marble::math::Mat4;
using marble::render::IRenderBackend;
using marble::render::MeshDrawInstance;
using marble::render::VulkanRhi;
using marble::render::kPcFlagClipSpace;
using marble::render::kPcFlagUnlit;

constexpr int kMenuRowBaseY = 120;
constexpr int kMenuRowStep = 32;
/// Ignore menu confirm/navigation briefly so a held Enter from "Run" / OS focus cannot boot Marbles on frame 0.
constexpr unsigned kMenuInputDelayFrames = 24;

struct LandingState {
    explicit LandingState(Engine& engineIn) : engine(engineIn) {}

    Engine& engine;
    VulkanRhi rhi;
    marble::core::BinaryResourceManager<64> assetRegistry_{};

    std::uint32_t meshUi = std::numeric_limits<std::uint32_t>::max();
    int fbW = 0;
    int fbH = 0;
    int selection = 0;
    std::string statusLine{};
    bool prevUp = false;
    bool prevDown = false;
    bool prevEnter = false;
    bool prevEsc = false;
    bool meshDirty = true;
    unsigned landingFrame_ = 0;
    PostLandingAction outcome = PostLandingAction::Quit;

    [[nodiscard]] bool rebuildUiMesh() {
        if (!engine.window()) {
            return false;
        }
        engine.window()->getFramebufferSize(&fbW, &fbH);
        if (fbW <= 0 || fbH <= 0) {
            return false;
        }

        std::vector<VulkanRhi::Vertex> vtx;
        std::vector<std::uint32_t> idx;
        float const hlTop = static_cast<float>(kMenuRowBaseY + selection * kMenuRowStep - 6);
        float const hlBot = static_cast<float>(kMenuRowBaseY + selection * kMenuRowStep + 22);
        marble::ui::appendNdcUnlitScreenRectPx(
            36.f,
            hlTop,
            static_cast<float>(fbW - 36),
            hlBot,
            fbW,
            fbH,
            0.12f,
            0.2f,
            0.38f,
            vtx,
            idx
        );

        marble::ui::appendEasyFontTextPx(
            "Welcome to Marble",
            48.f,
            48.f,
            fbW,
            fbH,
            0.92f,
            0.93f,
            0.95f,
            vtx,
            idx
        );
        marble::ui::appendEasyFontTextPx(
            "Choose a mode. Garden join connects to a garden_server.",
            48.f,
            80.f,
            fbW,
            fbH,
            0.75f,
            0.8f,
            0.85f,
            vtx,
            idx
        );
        marble::ui::appendEasyFontTextPx(
            "UP/DOWN and ENTER. In game: Esc then Q returns here.",
            48.f,
            100.f,
            fbW,
            fbH,
            0.55f,
            0.62f,
            0.68f,
            vtx,
            idx
        );

        marble::ui::appendEasyFontTextPx(
            "Marbles - new game",
            56.f,
            static_cast<float>(kMenuRowBaseY),
            fbW,
            fbH,
            0.9f,
            0.88f,
            0.82f,
            vtx,
            idx
        );
        marble::ui::appendEasyFontTextPx(
            "Garden - new game",
            56.f,
            static_cast<float>(kMenuRowBaseY + kMenuRowStep),
            fbW,
            fbH,
            0.82f,
            0.9f,
            0.85f,
            vtx,
            idx
        );
        marble::ui::appendEasyFontTextPx(
            "Garden - listen host (dev)",
            56.f,
            static_cast<float>(kMenuRowBaseY + 2 * kMenuRowStep),
            fbW,
            fbH,
            0.72f,
            0.85f,
            0.78f,
            vtx,
            idx
        );
        marble::ui::appendEasyFontTextPx(
            "Garden - join server (127.0.0.1:27778)",
            56.f,
            static_cast<float>(kMenuRowBaseY + 3 * kMenuRowStep),
            fbW,
            fbH,
            0.65f,
            0.82f,
            0.9f,
            vtx,
            idx
        );
        marble::ui::appendEasyFontTextPx(
            "Quit",
            56.f,
            static_cast<float>(kMenuRowBaseY + 4 * kMenuRowStep),
            fbW,
            fbH,
            0.75f,
            0.72f,
            0.7f,
            vtx,
            idx
        );

        if (!statusLine.empty()) {
            marble::ui::appendEasyFontTextPx(statusLine, 48.f, 268.f, fbW, fbH, 0.95f, 0.7f, 0.35f, vtx, idx);
        }

        if (vtx.empty() || idx.empty()) {
            return false;
        }

        if (meshUi == std::numeric_limits<std::uint32_t>::max()) {
            meshUi = rhi.uploadMesh(std::span<VulkanRhi::Vertex const>(vtx.data(), vtx.size()), idx);
            if (meshUi == std::numeric_limits<std::uint32_t>::max()) {
                return false;
            }
        } else {
            if (!rhi.replaceMesh(meshUi, std::span<VulkanRhi::Vertex const>(vtx.data(), vtx.size()), idx)) {
                return false;
            }
        }
        return true;
    }

    void step(Engine::FrameContext const& ctx) {
        (void)ctx;
        auto* w = engine.window();
        if (!w) {
            return;
        }

        ++landingFrame_;

        int nw = 0;
        int nh = 0;
        w->getFramebufferSize(&nw, &nh);
        if (nw != fbW || nh != fbH) {
            meshDirty = true;
        }

        bool const up = w->isKeyDown(marble::platform::Key::Up);
        bool const down = w->isKeyDown(marble::platform::Key::Down);
        bool const enter = w->isKeyDown(marble::platform::Key::Enter);
        bool const esc = w->isKeyDown(marble::platform::Key::Escape);

        bool const menuReady = landingFrame_ >= kMenuInputDelayFrames;
        if (!menuReady) {
            // Sync edge detectors so the first frame we accept input has no stale "key just went down".
            prevUp = up;
            prevDown = down;
            prevEnter = enter;
            prevEsc = esc;
        } else {
            if (up && !prevUp) {
                selection = (selection + 4) % 5;
                meshDirty = true;
            }
            if (down && !prevDown) {
                selection = (selection + 1) % 5;
                meshDirty = true;
            }
            if (enter && !prevEnter) {
                if (selection == 0) {
                    outcome = PostLandingAction::Marbles;
                    engine.requestEndRun();
                } else if (selection == 1) {
                    outcome = PostLandingAction::Garden;
                    engine.requestEndRun();
                } else if (selection == 2) {
                    outcome = PostLandingAction::GardenListenHost;
                    engine.requestEndRun();
                } else if (selection == 3) {
                    outcome = PostLandingAction::GardenRemoteClient;
                    engine.requestEndRun();
                } else {
                    outcome = PostLandingAction::Quit;
                    engine.requestClose();
                }
            }
            if (esc && !prevEsc) {
                outcome = PostLandingAction::Quit;
                engine.requestClose();
            }

            prevUp = up;
            prevDown = down;
            prevEnter = enter;
            prevEsc = esc;
        }

        if (meshDirty) {
            if (rebuildUiMesh()) {
                meshDirty = false;
            }
        }
    }

    void render(Engine::FrameContext const& ctx) {
        (void)ctx;
        IRenderBackend& backend = rhi;
        Mat4 const viewProj = Mat4::identity();
        std::vector<MeshDrawInstance> draws;
        if (meshUi != std::numeric_limits<std::uint32_t>::max()) {
            draws.resize(1);
            MeshDrawInstance& d = draws[0];
            d.meshIndex = meshUi;
            d.model = Mat4::identity();
            d.color = {1.f, 1.f, 1.f};
            d.colorAlpha = 1.f;
            d.drawFlags = kPcFlagClipSpace | kPcFlagUnlit;
        }
        if (auto* win = engine.window()) {
            (void)backend.drawFrame(*win, viewProj, draws, nullptr);
        }
    }
};

class LandingScreen {
public:
    LandingScreen(Engine& engine, std::string shaderDirectory, std::optional<std::uint32_t> physicalDeviceIndex)
        : engine_(engine),
          shaderDirectory_(std::move(shaderDirectory)),
          physicalDeviceIndex_(physicalDeviceIndex),
          state_(std::make_unique<LandingState>(engine)) {}

    ~LandingScreen() {
        if (state_) {
            state_->rhi.shutdown();
        }
        engine_.resetPhasesToDefaults();
    }

    LandingScreen(LandingScreen const&) = delete;
    LandingScreen& operator=(LandingScreen const&) = delete;

    void installPhases() {
        auto* raw = state_.get();
        struct Sim final : public Engine::ISimulationPhase {
            explicit Sim(LandingState* s) : s_(s) {}
            void tick(Engine::FrameContext const& ctx) override {
                if (s_) {
                    s_->step(ctx);
                }
            }
            LandingState* s_;
        };
        struct Rend final : public Engine::IRenderPhase {
            explicit Rend(LandingState* s) : s_(s) {}
            void render(Engine::FrameContext const& ctx) override {
                if (s_) {
                    s_->render(ctx);
                }
            }
            LandingState* s_;
        };
        engine_.setSimulationPhase(std::make_unique<Sim>(raw));
        engine_.setRenderPhase(std::make_unique<Rend>(raw));
    }

    [[nodiscard]] bool initGraphics() {
        if (!engine_.window()) {
            return false;
        }
        bool vkOk = false;
        std::string const assetsRoot = engine_.assetsRootPath();
        if (!assetsRoot.empty()) {
            (void)marble::core::setBinaryResourceSearchRoot(state_->assetRegistry_, std::filesystem::path(assetsRoot));
            if (state_->assetRegistry_.acquire("shaders/mesh.vert.spv") &&
                state_->assetRegistry_.acquire("shaders/mesh.frag.spv")) {
                marble::core::BinaryResource const* const vertRes = state_->assetRegistry_.find("shaders/mesh.vert.spv");
                marble::core::BinaryResource const* const fragRes = state_->assetRegistry_.find("shaders/mesh.frag.spv");
                if (vertRes != nullptr && fragRes != nullptr) {
                    std::span<std::uint8_t const> const vspan(vertRes->bytes.data(), vertRes->bytes.size());
                    std::span<std::uint8_t const> const fspan(fragRes->bytes.data(), fragRes->bytes.size());
                    vkOk = state_->rhi.initFromSpirvBytes(
                        *engine_.window(), "Marble", vspan, fspan, physicalDeviceIndex_);
                }
            }
        }
        if (!vkOk) {
            if (!state_->rhi.init(*engine_.window(), "Marble", shaderDirectory_, physicalDeviceIndex_)) {
                return false;
            }
        }
        IRenderBackend& rb = state_->rhi;
        rb.setClearColor(0.04f, 0.06f, 0.1f, 1.f);
        if (auto* w = engine_.window()) {
            w->setTitle("Marble - main menu");
        }
        return true;
    }

    [[nodiscard]] PostLandingAction outcome() const { return state_->outcome; }

private:
    Engine& engine_;
    std::string shaderDirectory_;
    std::optional<std::uint32_t> physicalDeviceIndex_;
    std::unique_ptr<LandingState> state_;
};

} // namespace

int runWindowedGameLoop(
    Engine& engine,
    std::string const& shaderDirectory,
    std::optional<std::uint32_t> physicalDeviceIndex
) {
    for (;;) {
        PostLandingAction const choice = runLandingMenu(engine, shaderDirectory, physicalDeviceIndex);
        if (choice == PostLandingAction::Quit) {
            return 0;
        }
        int const sessionRc =
            runGameplaySession(engine, choice, shaderDirectory, physicalDeviceIndex, std::nullopt);
        if (sessionRc != 0) {
            return sessionRc;
        }
        if (engine.window() == nullptr) {
            return 0;
        }
        if (engine.window()->shouldClose()) {
            return 0;
        }
    }
}

PostLandingAction runLandingMenu(
    Engine& engine,
    std::string const& shaderDirectory,
    std::optional<std::uint32_t> physicalDeviceIndex
) {
    LandingScreen landing(engine, shaderDirectory, physicalDeviceIndex);
    landing.installPhases();
    if (!landing.initGraphics()) {
        return PostLandingAction::Quit;
    }
    (void)engine.run();
    return landing.outcome();
}

int runGameplaySession(
    Engine& engine,
    PostLandingAction mode,
    std::string const& shaderDirectory,
    std::optional<std::uint32_t> physicalDeviceIndex,
    std::optional<marble::garden_app::RemoteClientParams> gardenRemoteOptions
) {
    if (mode == PostLandingAction::Quit) {
        return 0;
    }
    if (mode == PostLandingAction::Marbles) {
        marble::marbles::MarblesGame game(engine);
        game.installPhases();
        if (engine.window() != nullptr) {
            if (!game.initGraphics(shaderDirectory, physicalDeviceIndex)) {
                return 1;
            }
        }
        return engine.run();
    }
    using marble::garden_app::GardenSessionKind;
    using marble::garden_app::RemoteClientParams;
    GardenSessionKind gardenSession = GardenSessionKind::Offline;
    RemoteClientParams clientParams{};
    if (gardenRemoteOptions.has_value()) {
        clientParams = *gardenRemoteOptions;
    }
    if (mode == PostLandingAction::GardenListenHost) {
        gardenSession = GardenSessionKind::ListenHost;
    } else if (mode == PostLandingAction::GardenRemoteClient) {
        gardenSession = GardenSessionKind::RemoteClient;
    }
    marble::garden_app::GardenGame game(engine, gardenSession, clientParams);
    game.installPhases();
    if (engine.window() != nullptr) {
        if (!game.initGraphics(shaderDirectory, physicalDeviceIndex)) {
            return 1;
        }
    }
    return engine.run();
}

} // namespace marble::marbles_app
