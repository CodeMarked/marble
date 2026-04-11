#pragma once

#include "core/Engine.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace marble::garden_app {

enum class GardenSessionKind : std::uint8_t {
    /// Single-player; offline `SessionConfig` (see `OnlineMultiplayerFoundation.hpp`).
    Offline,
    /// Listen-server topology bootstrapped (no transport yet); session/roster visible for development.
    ListenHost,
    /// Remote client: connects to a garden_server, receives and interpolates snapshots.
    RemoteClient,
};

/// Connection parameters for RemoteClient mode.
struct RemoteClientParams {
    std::string host = "127.0.0.1";
    std::uint16_t port = 27778u;
    /// Procedural garden layout; **must** match the dedicated server's `--seed` or the ball will look buried
    /// and the world will not match snapshots (`marble::garden::kGardenDedicatedServerDefaultLayoutSeed`).
    std::uint32_t layoutSeed = 42u;
    /// Must match dedicated server [`SessionConfig::joinTokenU32`] when the server requires a join token.
    std::uint32_t joinTokenU32 = 0u;
};

/// Phase-1 sample: fixed-step garden physics, two marbles, third-person camera, mouse flick.
class GardenGame {
public:
    struct State;

    explicit GardenGame(
        core::Engine& engine,
        GardenSessionKind session = GardenSessionKind::Offline,
        RemoteClientParams clientParams = {}
    );
    ~GardenGame();

    void installPhases();

    /// After `engine.init()` when not headless: same SPIR-V load path as `MarblesGame::initGraphics`.
    [[nodiscard]] bool initGraphics(
        std::string shaderDirectory,
        std::optional<std::uint32_t> physicalDeviceIndex = std::nullopt
    );

private:
    core::Engine& engine_;
    std::unique_ptr<State> state_;
};

} // namespace marble::garden_app
