#pragma once

#include "core/Engine.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace marble::marbles {

/// Wires tilt-board simulation and Vulkan rendering into `Engine` phases.
class MarblesGame {
public:
    struct State;

    explicit MarblesGame(core::Engine& engine);
    ~MarblesGame();

    /// After `engine.init()` when not headless: registry + `initFromSpirvBytes` if assets root resolves, else disk
    /// SPIR-V from paths in generated `MarbleSampleShaderNames.hpp` (driven by `MARBLE_SHADER_GLSL_MODULES`).
    [[nodiscard]] bool initGraphics(
        std::string shaderDirectory,
        std::optional<std::uint32_t> physicalDeviceIndex = std::nullopt
    );

    void installPhases();

private:
    core::Engine& engine_;
    std::unique_ptr<State> state_;
};

} // namespace marble::marbles
