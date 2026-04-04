#pragma once

#include <cstddef>

namespace marble::platform {

/// Operating-system virtual memory page size (bytes), e.g. 4096 on many desktops.
[[nodiscard]] std::size_t memoryPageSizeBytes() noexcept;

/// Hardware concurrency hint: logical processors available to the process, or 0 if unknown.
[[nodiscard]] unsigned int logicalProcessorCount() noexcept;

} // namespace marble::platform
