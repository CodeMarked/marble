#include "core/Parallelism.hpp"
#include "platform/hardware/System.hpp"

static_assert(!marble::core::engineSpawnsWorkerThreads());

int main() {
    const unsigned int n = marble::platform::logicalProcessorCount();
    const unsigned int w = marble::core::maxRecommendedWorkerThreads();
    if (n <= 1) {
        if (w != 0) {
            return 1;
        }
    } else {
        if (w != n - 1) {
            return 2;
        }
    }
    return 0;
}
