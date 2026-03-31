#pragma once

#include <memory>
#include <string>

namespace marble::platform {
class Window;
}

namespace marble::core {

/// Runtime owner for engine lifecycle and frame phases.
class Engine {
public:
    struct FrameContext {
        double deltaSeconds = 0.0;
        unsigned long long frameIndex = 0;
    };

    class ISimulationPhase {
    public:
        virtual ~ISimulationPhase() = default;
        virtual void tick(const FrameContext& ctx) = 0;
    };

    class IRenderPhase {
    public:
        virtual ~IRenderPhase() = default;
        virtual void render(const FrameContext& ctx) = 0;
    };

    struct Config {
        std::string appName = "Marble";
        int windowWidth = 1280;
        int windowHeight = 720;
        bool headless = false;
        unsigned long long maxFrames = 0;
        /// Interval for diagnostics output in seconds. Set <= 0 to disable.
        double diagnosticsIntervalSeconds = 1.0;
        /// When true, resolve a runtime assets directory (see `resolveAssetsRoot`).
        bool resolveAssetsRoot = false;
        /// Non-empty path tried after `MARBLE_ASSETS_ROOT` and before `<exe>/assets`.
        std::string assetsRootOverride;
        /// If true, `init()` fails when no valid assets root is found.
        bool requireAssetsDirectory = false;
    };

    explicit Engine(Config config);
    ~Engine();

    /// Initialize platform and runtime state.
    bool init();

    /// Run frame phases until close is requested.
    int run();

    /// Tear down runtime state in dependency-safe order.
    void shutdown();

    /// Optional phase injection points used to decouple runtime orchestration
    /// from concrete simulation and rendering implementations.
    void setSimulationPhase(std::unique_ptr<ISimulationPhase> simulationPhase);
    void setRenderPhase(std::unique_ptr<IRenderPhase> renderPhase);

    /// Empty when asset root resolution is disabled or no directory was found.
    const std::string& assetsRootPath() const;

    /// Non-owning pointer to the live window, or nullptr in headless mode or before `init()`.
    platform::Window* window();

    /// Stop `run()` after the current frame (ignored in headless mode without a window).
    void requestClose();

    /// Replace injected phases with the same defaults `init()` would use (noop render, fixed-step sim).
    void resetPhasesToDefaults();

private:
    void runFrame(double dtSeconds);
    void runDiagnostics(double dtSeconds);

    Config config_;
    bool initialized_ = false;
    bool closeRequested_ = false;
    unsigned long long frameCount_ = 0;
    double accumulatedTimeSeconds_ = 0.0;
    unsigned long long frameIndex_ = 0;
    std::unique_ptr<platform::Window> window_;
    std::unique_ptr<ISimulationPhase> simulationPhase_;
    std::unique_ptr<IRenderPhase> renderPhase_;
    std::string assetsRootResolved_;
};

} // namespace marble::core
