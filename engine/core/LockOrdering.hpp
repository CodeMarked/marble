#pragma once

#include <cstdint>

namespace marble::core {

/// Declared total order for multi-lock scenarios: acquire lower numeric levels before higher ones.
/// Extend only with project review (ADR-0014). Pair with Mutex / RecursiveMutex in SyncPrimitives.hpp;
/// use RecursiveMutex for re-entrant single-level sections instead of reordering.
enum class LockLevel : std::uint8_t {
    Diagnostics = 0,
    ResourceAccess = 1,
    SimulationState = 2,
};

/// Returns whether taking a lock associated with \p next is consistent with already holding \p held,
/// for **distinct** levels (`next` must be strictly greater than `held`).
[[nodiscard]] constexpr bool mayAcquireAfter(LockLevel held, LockLevel next) noexcept {
    return static_cast<std::uint8_t>(next) > static_cast<std::uint8_t>(held);
}

} // namespace marble::core
