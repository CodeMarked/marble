#include "core/MemoryLayout.hpp"

#include <cstddef>

using marble::core::alignDown;
using marble::core::alignUp;
using marble::core::isPowerOfTwo;
using marble::core::kCacheLineBytes;

static_assert(kCacheLineBytes == 64);

static_assert(isPowerOfTwo(1));
static_assert(isPowerOfTwo(64));
static_assert(!isPowerOfTwo(0));
static_assert(!isPowerOfTwo(3));

static_assert(alignUp(0, 16) == 0);
static_assert(alignUp(1, 16) == 16);
static_assert(alignUp(16, 16) == 16);
static_assert(alignUp(17, 16) == 32);

static_assert(alignDown(31, 16) == 16);
static_assert(alignDown(16, 16) == 16);
static_assert(alignDown(15, 16) == 0);

int main() {
    // Runtime smoke: formulas agree with constexpr checks on this platform.
    if (alignUp(1000, kCacheLineBytes) < 1000) {
        return 1;
    }
    if (alignDown(1000, kCacheLineBytes) > 1000) {
        return 2;
    }
    return 0;
}
