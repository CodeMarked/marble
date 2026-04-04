#include "platform/hardware/System.hpp"

#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace marble::platform {

std::size_t memoryPageSizeBytes() noexcept {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<std::size_t>(info.dwPageSize);
#else
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) {
        return 4096;
    }
    return static_cast<std::size_t>(page);
#endif
}

unsigned int logicalProcessorCount() noexcept {
    return std::thread::hardware_concurrency();
}

} // namespace marble::platform
