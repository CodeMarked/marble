#include "core/Parallelism.hpp"

#include "platform/hardware/System.hpp"

namespace marble::core {

unsigned int maxRecommendedWorkerThreads() noexcept {
    const unsigned int n = marble::platform::logicalProcessorCount();
    if (n <= 1) {
        return 0;
    }
    return n - 1;
}

} // namespace marble::core
