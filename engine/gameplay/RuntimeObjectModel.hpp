#pragma once

#include "math/Vec3.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

struct ObjectHandle {
    std::uint32_t index{};
    std::uint32_t generation{};

    [[nodiscard]] constexpr bool operator==(ObjectHandle const&) const noexcept = default;
};

struct RuntimeObject {
    std::uint32_t archetypeId{};
    math::Vec3 position{};
    bool enabled{true};
};

/// Fixed-capacity runtime object store with generational handles.
template <std::size_t MaxObjects>
class RuntimeObjectStore {
public:
    static_assert(MaxObjects > 0, "RuntimeObjectStore requires positive capacity");

    RuntimeObjectStore() noexcept {
        // Fill free list as a stack: 0..N-1 then pop from tail.
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(MaxObjects); ++i) {
            freeIndices_[i] = i;
        }
        freeCount_ = MaxObjects;
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return MaxObjects;
    }

    [[nodiscard]] std::size_t aliveCount() const noexcept {
        return aliveCount_;
    }

    [[nodiscard]] bool full() const noexcept {
        return freeCount_ == 0;
    }

    [[nodiscard]] ObjectHandle create(RuntimeObject object) noexcept {
        if (freeCount_ == 0) {
            return {};
        }
        const std::uint32_t index = freeIndices_[--freeCount_];
        occupied_[index] = true;
        objects_[index] = object;
        ++aliveCount_;
        return {index, generations_[index]};
    }

    [[nodiscard]] bool destroy(ObjectHandle handle) noexcept {
        if (!isAlive(handle)) {
            return false;
        }
        occupied_[handle.index] = false;
        objects_[handle.index] = {};
        ++generations_[handle.index];
        freeIndices_[freeCount_++] = handle.index;
        --aliveCount_;
        return true;
    }

    [[nodiscard]] bool isAlive(ObjectHandle handle) const noexcept {
        if (handle.index >= MaxObjects) {
            return false;
        }
        return occupied_[handle.index] && generations_[handle.index] == handle.generation;
    }

    [[nodiscard]] RuntimeObject* getMutable(ObjectHandle handle) noexcept {
        if (!isAlive(handle)) {
            return nullptr;
        }
        return &objects_[handle.index];
    }

    [[nodiscard]] RuntimeObject const* get(ObjectHandle handle) const noexcept {
        return const_cast<RuntimeObjectStore*>(this)->getMutable(handle);
    }

    template <typename Fn>
    void forEachAlive(Fn&& fn) {
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(MaxObjects); ++i) {
            if (occupied_[i]) {
                fn(ObjectHandle{i, generations_[i]}, objects_[i]);
            }
        }
    }

private:
    std::array<RuntimeObject, MaxObjects> objects_{};
    std::array<std::uint32_t, MaxObjects> generations_{};
    std::array<std::uint32_t, MaxObjects> freeIndices_{};
    std::array<bool, MaxObjects> occupied_{};
    std::size_t freeCount_{};
    std::size_t aliveCount_{};
};

} // namespace marble::gameplay
