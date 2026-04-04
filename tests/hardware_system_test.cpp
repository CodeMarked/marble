#include "core/MemoryLayout.hpp"
#include "platform/hardware/System.hpp"

int main() {
    using marble::core::isPowerOfTwo;
    const std::size_t page = marble::platform::memoryPageSizeBytes();
    if (!isPowerOfTwo(page) || page < 4096) {
        return 1;
    }

    // Contract: may be 0 if unknown (standard library); link-time smoke only here.
    (void)marble::platform::logicalProcessorCount();
    return 0;
}
