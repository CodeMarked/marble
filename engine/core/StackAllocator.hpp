#pragma once

#include "core/Assert.hpp"
#include "core/MemoryLayout.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace marble::core {

/// Linear stack allocator with marker/rollback API (book §6.2.1.1).
/// Allocation is bump-only; free is allowed only via `freeToMarker()` or `clear()`.
class StackAllocator {
public:
    using Marker = std::uint32_t;

    explicit StackAllocator(std::size_t capacityBytes)
        : buffer_(capacityBytes), top_(0) {}

    [[nodiscard]] std::size_t capacityBytes() const noexcept { return buffer_.size(); }
    [[nodiscard]] std::size_t usedBytes() const noexcept { return top_; }

    [[nodiscard]] Marker getMarker() const noexcept { return static_cast<Marker>(top_); }

    [[nodiscard]] void* alloc(std::size_t sizeBytes, std::size_t alignment = alignof(std::max_align_t)) noexcept {
        MARBLE_ASSERT(isPowerOfTwo(alignment));
        const std::size_t start = alignUp(top_, alignment);
        const std::size_t end = start + sizeBytes;
        if (end > buffer_.size()) {
            return nullptr;
        }
        top_ = end;
        return buffer_.data() + start;
    }

    void freeToMarker(Marker marker) noexcept {
        MARBLE_ASSERT(marker <= buffer_.size());
        top_ = static_cast<std::size_t>(marker);
    }

    void clear() noexcept { top_ = 0; }

private:
    std::vector<std::byte> buffer_{};
    std::size_t top_{0};
};

} // namespace marble::core
