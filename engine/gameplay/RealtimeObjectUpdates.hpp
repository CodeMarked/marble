#pragma once

#include "core/JobSystem.hpp"
#include "gameplay/RuntimeObjectModel.hpp"
#include "math/Vec3.hpp"

#include <array>
#include <cstddef>

namespace marble::gameplay {

struct UpdateContext {
    float deltaSeconds{};
    math::Vec3 gravity{0.f, 0.f, 0.f};
};

struct UpdateWorkRange {
    std::size_t begin{};
    std::size_t count{};
};

template <std::size_t MaxRanges>
[[nodiscard]] inline std::size_t buildUpdateWorkRanges(std::size_t totalCount,
                                                       std::size_t batchSize,
                                                       std::array<UpdateWorkRange, MaxRanges>& out) noexcept {
    if (batchSize == 0) {
        return 0;
    }
    std::size_t n = 0;
    core::job::scatterGather(totalCount, batchSize, [&out, &n](std::size_t begin, std::size_t count) {
        if (n < MaxRanges) {
            out[n++] = UpdateWorkRange{begin, count};
        }
    });
    return n;
}

template <std::size_t MaxObjects>
inline void integrateObjectPositions(RuntimeObjectStore<MaxObjects>& store,
                                     ObjectHandle const* orderedHandles,
                                     std::size_t handleCount,
                                     UpdateContext const& ctx) noexcept {
    if (orderedHandles == nullptr || ctx.deltaSeconds <= 0.f) {
        return;
    }
    const math::Vec3 delta = ctx.gravity * (0.5f * ctx.deltaSeconds * ctx.deltaSeconds);
    for (std::size_t i = 0; i < handleCount; ++i) {
        RuntimeObject* object = store.getMutable(orderedHandles[i]);
        if (object == nullptr || !object->enabled) {
            continue;
        }
        object->position = object->position + delta;
    }
}

template <std::size_t MaxObjects>
[[nodiscard]] inline std::size_t collectAliveHandles(RuntimeObjectStore<MaxObjects>& store,
                                                     ObjectHandle* out,
                                                     std::size_t outCapacity) noexcept {
    if (out == nullptr || outCapacity == 0u) {
        return 0u;
    }
    std::size_t n = 0;
    store.forEachAlive([&out, &n, outCapacity](ObjectHandle handle, RuntimeObject&) {
        if (n < outCapacity) {
            out[n] = handle;
            ++n;
        }
    });
    return n;
}

} // namespace marble::gameplay
