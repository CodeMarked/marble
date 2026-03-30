#include "platform/paths/ExecutableDir.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace marble::platform {

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    std::filesystem::path path(buffer);
    return path.parent_path();
#else
    return {};
#endif
}

} // namespace marble::platform
