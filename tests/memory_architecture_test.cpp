#include "core/MemoryArchitecture.hpp"
#include "core/MemoryLayout.hpp"

static_assert(marble::core::kCacheLineBytes == 64);
static_assert(alignof(marble::core::CacheLinePad) == marble::core::kCacheLineBytes);
static_assert(sizeof(marble::core::CacheLinePad) == marble::core::kCacheLineBytes);

int main() {
    return 0;
}
