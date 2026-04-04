#pragma once

namespace marble::core {

/// Whether the engine creates worker threads for frame work. `false` for this milestone (ADR-0001, ADR-0011).
[[nodiscard]] constexpr bool engineSpawnsWorkerThreads() noexcept {
    return false;
}

/// Upper bound for future explicit worker threads (one logical CPU reserved for main thread / OS).
/// Uses `marble::platform::logicalProcessorCount()`; returns 0 when unknown or when there is no headroom.
[[nodiscard]] unsigned int maxRecommendedWorkerThreads() noexcept;

} // namespace marble::core
