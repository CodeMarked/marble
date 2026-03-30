#include "platform/os/ProcessId.hpp"

#include <cstdint>

int main() {
    const std::uint32_t pid = marble::platform::currentProcessId();
    if (pid == 0) {
        return 1;
    }
    return 0;
}
