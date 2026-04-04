#pragma once

#include <cstdarg>
#include <cstdint>

namespace marble::core {

/// Bit flags for log routing (book §10.1.3 channel model).
enum class LogChannel : std::uint32_t {
    General = 1u << 0,
    Engine = 1u << 1,
    Render = 1u << 2,
    Input = 1u << 3,
};

using LogChannelMask = std::uint32_t;

[[nodiscard]] constexpr LogChannelMask allLogChannels() noexcept {
    return 0xFFFFFFFFu;
}

using LogSink = void (*)(void* userData, char const* text);

/// Default: stderr (and OutputDebugString on Windows in implementation).
void setLogSink(LogSink sink, void* userData) noexcept;

[[nodiscard]] int globalLogVerbosity() noexcept;
void setGlobalLogVerbosity(int level) noexcept;

[[nodiscard]] LogChannelMask globalLogChannelFilter() noexcept;
void setGlobalLogChannelFilter(LogChannelMask mask) noexcept;

/// Prints when `verbosity <= globalLogVerbosity()` and channel bits overlap the filter.
[[nodiscard]] int logPrintf(int verbosity, LogChannelMask channels, char const* format, ...);
[[nodiscard]] int vlogPrintf(int verbosity, LogChannelMask channels, char const* format, std::va_list args) noexcept;

} // namespace marble::core
