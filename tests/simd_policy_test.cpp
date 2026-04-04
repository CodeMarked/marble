#include "core/MemoryLayout.hpp"
#include "core/Simd.hpp"

#include <cstddef>

static_assert(marble::core::kSimd128AlignBytes == 16);
static_assert(marble::core::kSimd256AlignBytes == 32);
static_assert(marble::core::kSimd512AlignBytes == 64);

static_assert(marble::core::simdBufferAlignBytes(128) == 16);
static_assert(marble::core::simdBufferAlignBytes(256) == 32);
static_assert(marble::core::simdBufferAlignBytes(512) == 64);

static_assert(marble::core::alignUp(0, marble::core::kSimd256AlignBytes) == 0);
static_assert(marble::core::alignUp(100, marble::core::kSimd256AlignBytes) == 128);

int main() {
    return 0;
}
