#include "platform/paths/ExecutableDir.hpp"

#include <cstddef>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

#include <filesystem>
#include <system_error>

namespace marble::platform {

namespace {

#if defined(__linux__)

[[nodiscard]] std::filesystem::path linuxExecutablePath() {
    std::size_t cap = static_cast<std::size_t>(PATH_MAX);
    if (cap < 512) {
        cap = 512;
    }
    for (;;) {
        std::vector<char> buf(cap);
        ssize_t const len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
        if (len < 0) {
            return {};
        }
        if (static_cast<std::size_t>(len) >= buf.size() - 1) {
            cap *= 2;
            if (cap > 1u << 20) {
                return {};
            }
            continue;
        }
        buf[static_cast<std::size_t>(len)] = '\0';
        return std::filesystem::path(buf.data());
    }
}

#endif

#if defined(__APPLE__)

[[nodiscard]] std::filesystem::path macosExecutablePath() {
    std::uint32_t bufsize = 0;
    if (_NSGetExecutablePath(nullptr, &bufsize) != -1) {
        return {};
    }
    if (bufsize == 0) {
        return {};
    }
    std::vector<char> buf(static_cast<std::size_t>(bufsize));
    if (_NSGetExecutablePath(buf.data(), &bufsize) != 0) {
        return {};
    }
    return std::filesystem::path(buf.data());
}

#endif

} // namespace

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    std::filesystem::path const path(buffer);
    return path.parent_path();
#elif defined(__linux__)
    std::filesystem::path const exe = linuxExecutablePath();
    if (exe.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::path const canon = std::filesystem::weakly_canonical(exe, ec);
    if (ec) {
        return exe.parent_path();
    }
    return canon.parent_path();
#elif defined(__APPLE__)
    std::filesystem::path const exe = macosExecutablePath();
    if (exe.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::path const canon = std::filesystem::weakly_canonical(exe, ec);
    if (ec) {
        return exe.parent_path();
    }
    return canon.parent_path();
#else
    // Other POSIX: no portable default yet (see docs/ENGINE_ROADMAP.md §4 *Platform validation*).
    return {};
#endif
}

} // namespace marble::platform
