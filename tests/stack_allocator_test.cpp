#include "core/StackAllocator.hpp"

#include <cstdint>
#include <cstdlib>

int main() {
    marble::core::StackAllocator alloc(128);

    if (alloc.capacityBytes() != 128 || alloc.usedBytes() != 0) {
        return 1;
    }

    const auto m0 = alloc.getMarker();
    void* a = alloc.alloc(16, 8);
    if (a == nullptr) {
        return 2;
    }
    if ((reinterpret_cast<std::uintptr_t>(a) % 8u) != 0u) {
        return 3;
    }

    const auto m1 = alloc.getMarker();
    void* b = alloc.alloc(24, 16);
    if (b == nullptr) {
        return 4;
    }
    if ((reinterpret_cast<std::uintptr_t>(b) % 16u) != 0u) {
        return 5;
    }

    const std::size_t usedAfterB = alloc.usedBytes();
    alloc.freeToMarker(m1);
    if (alloc.usedBytes() > usedAfterB || alloc.usedBytes() < m1) {
        return 6;
    }

    void* c = alloc.alloc(8, 8);
    if (c != b) {
        return 7;
    }

    alloc.freeToMarker(m0);
    if (alloc.usedBytes() != 0) {
        return 8;
    }

    void* big = alloc.alloc(1024, 8);
    if (big != nullptr) {
        return 9;
    }

    void* d = alloc.alloc(32, 8);
    if (d == nullptr) {
        return 10;
    }
    alloc.clear();
    if (alloc.usedBytes() != 0) {
        return 11;
    }

    return 0;
}
