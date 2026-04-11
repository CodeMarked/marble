#pragma once

#include <cstdint>

namespace marble::render {

/// Documented baseline for scaling Vulkan + shipped samples ([ADR-0057]).
/// Build-time `glslc` (shaderc CLI) remains the default compile path; optional migration to
/// libshaderc or fully offline packaged SPIR-V is a deliberate product decision, not a code fork here.
enum class ShaderCompileStrategy : std::uint8_t {
    BuildTimeGlslc = 0,
    LibShaderc = 1,
    PrecompiledSpirvOnly = 2
};

inline constexpr ShaderCompileStrategy kActiveShaderCompileStrategy = ShaderCompileStrategy::BuildTimeGlslc;

} // namespace marble::render
