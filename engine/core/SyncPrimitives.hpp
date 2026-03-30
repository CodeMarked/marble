#pragma once

#include <condition_variable>
#include <mutex>

namespace marble::core {

/// Vocabulary for lock-based synchronization (Ch. 4.5–4.6). Prefer these aliases in engine code
/// so policy changes stay centralized (ADR-0013). Multi-lock ordering: see LockOrdering.hpp (ADR-0014).

using Mutex = std::mutex;
using RecursiveMutex = std::recursive_mutex;

template <typename M>
using LockGuard = std::lock_guard<M>;

template <typename M>
using UniqueLock = std::unique_lock<M>;

using ConditionVariable = std::condition_variable;

} // namespace marble::core
