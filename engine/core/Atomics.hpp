#pragma once

#include <atomic>
#include <cstdint>

namespace marble::core {

/// Lock-free atomics vocabulary (Ch. 4.9). Use `std::atomic` through these aliases so memory-order
/// policy and future instrumentation stay centralized (ADR-0015).

template <typename T>
using Atomic = std::atomic<T>;

using AtomicU32 = std::atomic<std::uint32_t>;
using AtomicU64 = std::atomic<std::uint64_t>;

} // namespace marble::core
