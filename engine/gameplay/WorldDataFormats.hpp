#pragma once

#include "gameplay/RuntimeObjectModel.hpp"
#include "math/Geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

struct ChunkCoord {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};

    [[nodiscard]] constexpr bool operator==(ChunkCoord const&) const noexcept = default;
};

struct WorldObjectRef {
    std::uint64_t guid{};

    [[nodiscard]] constexpr bool operator==(WorldObjectRef const&) const noexcept = default;
};

inline constexpr WorldObjectRef kInvalidWorldObjectRef{};

[[nodiscard]] inline bool isValid(WorldObjectRef ref) noexcept {
    return ref.guid != 0u;
}

/// Server replication / physics budget class for chunk objects; see docs/architecture/multiplayer-physics-world-scale.md.
enum class ObjectSimulationClass : std::uint8_t {
    Static = 0,
    Sleepable = 1,
    Dynamic = 2,
    Cosmetic = 3
};

struct ChunkObjectRecord {
    WorldObjectRef self{};
    std::uint32_t archetypeId{};
    math::Vec3 position{};
    WorldObjectRef parent{}; // optional parent link for graph reconstruction
    ObjectSimulationClass simulationClass{ObjectSimulationClass::Static};
};

template <std::size_t MaxRecords>
struct WorldChunkData {
    ChunkCoord coord{};
    std::array<ChunkObjectRecord, MaxRecords> records{};
    std::size_t recordCount{};
};

[[nodiscard]] constexpr std::uint64_t packChunkCoordKey(ChunkCoord c) noexcept {
    // 21 bits each axis + sign bias; deterministic packed key for hash maps.
    const std::uint64_t bx = static_cast<std::uint64_t>(static_cast<std::int64_t>(c.x) + 1048576);
    const std::uint64_t by = static_cast<std::uint64_t>(static_cast<std::int64_t>(c.y) + 1048576);
    const std::uint64_t bz = static_cast<std::uint64_t>(static_cast<std::int64_t>(c.z) + 1048576);
    return (bx & 0x1fffffULL) | ((by & 0x1fffffULL) << 21u) | ((bz & 0x1fffffULL) << 42u);
}

template <std::size_t MaxRecords>
[[nodiscard]] inline bool appendRecord(WorldChunkData<MaxRecords>& chunk, ChunkObjectRecord const& record) noexcept {
    if (chunk.recordCount >= MaxRecords) {
        return false;
    }
    chunk.records[chunk.recordCount++] = record;
    return true;
}

template <std::size_t MaxRecords>
[[nodiscard]] inline ChunkObjectRecord const* findRecordByGuid(WorldChunkData<MaxRecords> const& chunk,
                                                               WorldObjectRef id) noexcept {
    for (std::size_t i = 0; i < chunk.recordCount; ++i) {
        if (chunk.records[i].self == id) {
            return &chunk.records[i];
        }
    }
    return nullptr;
}

template <std::size_t MaxRecords, std::size_t MaxOut>
[[nodiscard]] inline std::size_t queryRecordsInAabb(WorldChunkData<MaxRecords> const& chunk,
                                                    math::Aabb const& query,
                                                    std::array<WorldObjectRef, MaxOut>& out) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < chunk.recordCount; ++i) {
        if (!math::contains(query, chunk.records[i].position)) {
            continue;
        }
        if (n < MaxOut) {
            out[n] = chunk.records[i].self;
        }
        ++n;
    }
    return n;
}

} // namespace marble::gameplay
