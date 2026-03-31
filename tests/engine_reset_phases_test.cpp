#include "core/Engine.hpp"

int main()
{
    marble::core::Engine::Config cfg {};
    cfg.headless = true;
    cfg.maxFrames = 2;
    cfg.diagnosticsIntervalSeconds = 0.0;
    cfg.resolveAssetsRoot = false;

    marble::core::Engine engine(cfg);
    if (!engine.init()) {
        return 1;
    }

    engine.resetPhasesToDefaults();

    if (engine.run() != 0) {
        return 2;
    }

    engine.shutdown();
    return 0;
}
