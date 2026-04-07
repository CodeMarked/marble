#pragma once

#include "core/Engine.hpp"

#include <optional>
#include <string>

namespace marble::marbles_app {

enum class PostLandingAction { Quit, Marbles, Garden, GardenListenHost, GardenRemoteClient };

/// Windowed: main menu then Marbles or Garden in a loop; Esc pause, then Q returns to this menu; window close exits.
[[nodiscard]] int runWindowedGameLoop(
    core::Engine& engine,
    std::string const& shaderDirectory,
    std::optional<std::uint32_t> physicalDeviceIndex
);

/// Windowed menu: returns chosen mode or Quit. Used by `runWindowedGameLoop`.
[[nodiscard]] PostLandingAction runLandingMenu(
    core::Engine& engine,
    std::string const& shaderDirectory,
    std::optional<std::uint32_t> physicalDeviceIndex
);

/// Runs one gameplay session until `run()` ends (`requestEndRun`, close, etc.).
[[nodiscard]] int runGameplaySession(
    core::Engine& engine,
    PostLandingAction mode,
    std::string const& shaderDirectory,
    std::optional<std::uint32_t> physicalDeviceIndex
);

} // namespace marble::marbles_app
