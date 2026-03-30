#include "core/Log.hpp"

#include <array>
#include <cstdio>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace marble::core {

namespace {

constexpr std::size_t kBufferChars = 1024;

LogSink g_sink = nullptr;
void* g_sinkUser = nullptr;

int g_verbosity = 0;
LogChannelMask g_channelFilter = allLogChannels();

void defaultPlatformSink(void*, char const* text) {
    if (text == nullptr) {
        return;
    }
    std::fputs(text, stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
#ifdef _WIN32
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
#endif
}

void dispatchLine(char const* line) {
    if (g_sink != nullptr) {
        g_sink(g_sinkUser, line);
    } else {
        defaultPlatformSink(nullptr, line);
    }
}

} // namespace

void setLogSink(LogSink sink, void* userData) noexcept {
    g_sink = sink;
    g_sinkUser = userData;
}

int globalLogVerbosity() noexcept {
    return g_verbosity;
}

void setGlobalLogVerbosity(int level) noexcept {
    g_verbosity = level;
}

LogChannelMask globalLogChannelFilter() noexcept {
    return g_channelFilter;
}

void setGlobalLogChannelFilter(LogChannelMask mask) noexcept {
    g_channelFilter = mask;
}

int vlogPrintf(int verbosity, LogChannelMask channels, char const* format, std::va_list args) noexcept {
    if (format == nullptr) {
        return 0;
    }
    if (verbosity > g_verbosity) {
        return 0;
    }
    if (channels != 0u && (channels & g_channelFilter) == 0u) {
        return 0;
    }

    std::array<char, kBufferChars> buffer{};
    const int n = std::vsnprintf(buffer.data(), buffer.size(), format, args);
    if (n < 0) {
        return n;
    }
    if (static_cast<std::size_t>(n) >= buffer.size()) {
        buffer[buffer.size() - 1] = '\0';
    }
    dispatchLine(buffer.data());
    return n;
}

int logPrintf(int verbosity, LogChannelMask channels, char const* format, ...) {
    std::va_list ap;
    va_start(ap, format);
    const int r = vlogPrintf(verbosity, channels, format, ap);
    va_end(ap);
    return r;
}

} // namespace marble::core
