#include "core/Engine.hpp"

int main() {
    marble::core::Engine::Config config {};
    config.appName = "MarbleSmokeTest";
    config.headless = true;
    config.maxFrames = 5;

    marble::core::Engine engine(config);
    if (!engine.init()) {
        return 1;
    }

    const int runCode = engine.run();
    engine.shutdown();
    return runCode;
}
