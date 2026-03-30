#include "core/Engine.hpp"
#include "core/Assert.hpp"
#include "core/AssetRoot.hpp"
#include "core/Log.hpp"
#include "core/Simulation.hpp"
#include "core/Time.hpp"

#include "platform/window/Window.hpp"

#include <chrono>
#include <memory>
#include <utility>

namespace marble::core {

namespace {

constexpr LogChannelMask kEngineLog = static_cast<LogChannelMask>(LogChannel::Engine);

class NoopRenderPhase final : public Engine::IRenderPhase {
public:
    void render(const Engine::FrameContext& ctx) override {
        (void)ctx;
    }
};

} // namespace

Engine::Engine(Config config) : config_(std::move(config)) {}

Engine::~Engine() {
    shutdown();
}

bool Engine::init() {
    if (initialized_) {
        return true;
    }

    if (config_.headless) {
        (void)logPrintf(0, kEngineLog, "Engine init: headless mode enabled");
    } else {
        (void)logPrintf(
            0,
            kEngineLog,
            "Engine init: creating window (%dx%d)",
            config_.windowWidth,
            config_.windowHeight
        );
        window_ = platform::Window::create(config_.appName, config_.windowWidth, config_.windowHeight);
        if (!window_) {
            (void)logPrintf(0, kEngineLog, "Engine init failed: window creation failed");
            return false;
        }
    }

    assetsRootResolved_.clear();
    if (config_.resolveAssetsRoot) {
        const auto root = resolveAssetsRoot(true, config_.assetsRootOverride, config_.requireAssetsDirectory);
        if (config_.requireAssetsDirectory && !root.has_value()) {
            window_.reset();
            return false;
        }
        if (root.has_value()) {
            assetsRootResolved_ = root->generic_string();
            (void)logPrintf(0, kEngineLog, "Engine assets root: %s", assetsRootResolved_.c_str());
        }
    }

    initialized_ = true;
    closeRequested_ = false;
    frameIndex_ = 0;
    if (!simulationPhase_) {
        simulationPhase_ = std::make_unique<FixedStepSimulationPhase>();
    }
    if (!renderPhase_) {
        renderPhase_ = std::make_unique<NoopRenderPhase>();
    }
    (void)logPrintf(0, kEngineLog, "Engine init complete");
    return true;
}

int Engine::run() {
    if (!initialized_) {
        (void)logPrintf(0, kEngineLog, "Engine run rejected: init() was not called");
        return 1;
    }

    using clock = std::chrono::steady_clock;
    auto lastFrameTime = clock::now();
    FrameDeltaEstimator<4> dtEstimator(1.0 / 60.0, 1.0);
    frameCount_ = 0;
    accumulatedTimeSeconds_ = 0.0;

    while (!closeRequested_) {
        if (window_ && window_->shouldClose()) {
            break;
        }

        auto now = clock::now();
        const double measuredDtSeconds = std::chrono::duration<double>(now - lastFrameTime).count();
        lastFrameTime = now;
        const double dtSeconds = dtEstimator.next(measuredDtSeconds);
        runFrame(dtSeconds);

        if (config_.maxFrames > 0 && frameCount_ >= config_.maxFrames) {
            closeRequested_ = true;
        }
    }

    return 0;
}

void Engine::shutdown() {
    if (!initialized_) {
        return;
    }

    (void)logPrintf(0, kEngineLog, "Engine shutdown: releasing runtime state");
    window_.reset();
    simulationPhase_.reset();
    renderPhase_.reset();
    assetsRootResolved_.clear();
    closeRequested_ = false;
    initialized_ = false;
    (void)logPrintf(0, kEngineLog, "Engine shutdown complete");
}

void Engine::setSimulationPhase(std::unique_ptr<ISimulationPhase> simulationPhase) {
    if (simulationPhase) {
        simulationPhase_ = std::move(simulationPhase);
    }
}

void Engine::setRenderPhase(std::unique_ptr<IRenderPhase> renderPhase) {
    if (renderPhase) {
        renderPhase_ = std::move(renderPhase);
    }
}

const std::string& Engine::assetsRootPath() const {
    return assetsRootResolved_;
}

void Engine::runFrame(double dtSeconds) {
    MARBLE_ASSERT(simulationPhase_);
    MARBLE_ASSERT(renderPhase_);

    // Phase 1: input/platform event pump.
    if (window_) {
        window_->pollEvents();
    }

    const FrameContext ctx{dtSeconds, frameIndex_};

    // Phase 2: simulation tick via injected interface.
    simulationPhase_->tick(ctx);

    // Phase 3: render tick via injected interface.
    renderPhase_->render(ctx);

    // Phase 4: diagnostics hook.
    runDiagnostics(dtSeconds);
    ++frameIndex_;
}

void Engine::runDiagnostics(double dtSeconds) {
    if (config_.diagnosticsIntervalSeconds <= 0.0) {
        return;
    }

    ++frameCount_;
    accumulatedTimeSeconds_ += dtSeconds;

    // Keep diagnostics low-noise while still proving the phase exists.
    if (accumulatedTimeSeconds_ >= config_.diagnosticsIntervalSeconds) {
        const double fps = static_cast<double>(frameCount_) / accumulatedTimeSeconds_;
        (void)logPrintf(
            0,
            kEngineLog,
            "Engine diagnostics: avg_fps=%f over %fs",
            fps,
            accumulatedTimeSeconds_
        );
        frameCount_ = 0;
        accumulatedTimeSeconds_ = 0.0;
    }
}

} // namespace marble::core
