#include "core/Atomics.hpp"

#include <cstdint>
#include <thread>

int main() {
    marble::core::AtomicU64 counter{};
    constexpr int kThreads = 2;
    constexpr int kIncr = 100000;

    std::thread a([&]() {
        for (int i = 0; i < kIncr; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    });
    std::thread b([&]() {
        for (int i = 0; i < kIncr; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    });
    a.join();
    b.join();

    if (counter.load(std::memory_order_relaxed) != static_cast<std::uint64_t>(kThreads * kIncr)) {
        return 1;
    }
    return 0;
}
