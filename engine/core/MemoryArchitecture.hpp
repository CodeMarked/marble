#pragma once

#include "core/MemoryLayout.hpp"

#include <cstddef>

namespace marble::core {

/// Opaque block aligned to `kCacheLineBytes` in `MemoryLayout.hpp`.
/// Place between independently mutated fields (for example counters or atomics on different threads)
/// to reduce **false sharing** when those fields would otherwise live in the same cache line.
///
/// This does not encode L1/L2/L3 sizes; hierarchy-specific tuning belongs in measured hot paths later.
struct CacheLinePad {
    alignas(kCacheLineBytes) unsigned char storage[kCacheLineBytes];
};

} // namespace marble::core
