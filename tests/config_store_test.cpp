#include "core/ConfigStore.hpp"

#include <cstdlib>
#include <string>

int main() {
    marble::core::ConfigStore<64> cfg;

    cfg.loadKeyValueText(R"(
        # comments
        ; comments
        [graphics]
        width = 1280
        height = 720
        fullscreen = false
        gamma = 2.2
        name = Marble
    )");

    int width = 0;
    int height = 0;
    float gamma = 0.f;
    bool fullscreen = true;
    std::string name;
    if (!cfg.getInt("width", width) || width != 1280) {
        return 1;
    }
    if (!cfg.getInt("height", height) || height != 720) {
        return 2;
    }
    if (!cfg.getFloat("gamma", gamma) || gamma < 2.19f || gamma > 2.21f) {
        return 3;
    }
    if (!cfg.getBool("fullscreen", fullscreen) || fullscreen) {
        return 4;
    }
    if (!cfg.getString("name", name) || name != "Marble") {
        return 5;
    }

    const char* argv[] = {"marbles", "--width=1920", "--vsync=on", "--badflag"};
    cfg.applyCommandLine(4, argv);
    if (!cfg.getInt("width", width) || width != 1920) {
        return 6;
    }
    bool vsync = false;
    if (!cfg.getBool("vsync", vsync) || !vsync) {
        return 7;
    }

    if (!cfg.set("lives", "3")) {
        return 8;
    }
    int lives = 0;
    if (!cfg.getInt("lives", lives) || lives != 3) {
        return 9;
    }

    return 0;
}
