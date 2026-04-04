#pragma once

#include <cstdint>

namespace marble::platform {

/// Operating-system process identifier for the running executable (logging / diagnostics).
/// Normal user processes are non-zero on Windows and POSIX targets used by Marble.
[[nodiscard]] std::uint32_t currentProcessId() noexcept;

} // namespace marble::platform
