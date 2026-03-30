#include "core/SyncPrimitives.hpp"

#include <thread>
#include <vector>

int main() {
    using marble::core::LockGuard;
    using marble::core::Mutex;

    Mutex mutex;
    int shared = 0;
    constexpr int kThreads = 4;
    constexpr int kIncr = 1000;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kThreads));
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < kIncr; ++j) {
                const LockGuard<Mutex> lock(mutex);
                ++shared;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    if (shared != kThreads * kIncr) {
        return 1;
    }
    return 0;
}
