#pragma once

#include "core/Engine.hpp"

#include <memory>
#include <optional>
#include <string>

namespace marble::garden_app {

enum class GardenSessionKind : std::uint8_t {
    /// Single-player; offline `SessionConfig` (see `OnlineMultiplayerFoundation.hpp`).
    Offline,
    /// Listen-server topology bootstrapped (no transport yet); session/roster visible for development.
    ListenHost,
};

/// Phase-1 sample: fixed-step garden physics, two marbles, third-person camera, mouse flick.
class GardenGame {
public:
    struct State;

    explicit GardenGame(core::Engine& engine, GardenSessionKind session = GardenSessionKind::Offline);
    ~GardenGame();

    void installPhases();

    [[nodiscard]] bool initGraphics(
        std::string shaderDirectory,
        std::optional<std::uint32_t> physicalDeviceIndex = std::nullopt
    );

private:
    core::Engine& engine_;
    std::unique_ptr<State> state_;
};

} // namespace marble::garden_app
