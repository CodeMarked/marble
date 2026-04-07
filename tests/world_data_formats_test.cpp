#include "gameplay/WorldDataFormats.hpp"

#include <array>

int main() {
    using namespace marble;
    using namespace marble::gameplay;

    WorldChunkData<3> chunk{};
    chunk.coord = {1, -2, 3};

    const std::uint64_t keyA = packChunkCoordKey(chunk.coord);
    const std::uint64_t keyB = packChunkCoordKey({1, -2, 3});
    const std::uint64_t keyC = packChunkCoordKey({1, -2, 4});
    if (keyA != keyB || keyA == keyC) {
        return 1;
    }

    if (isValid(kInvalidWorldObjectRef)) {
        return 2;
    }

    const ChunkObjectRecord r1{{101u}, 10u, {0.f, 0.f, 0.f}, {}};
    const ChunkObjectRecord r2{{102u}, 20u, {5.f, 0.f, 0.f}, {101u}};
    const ChunkObjectRecord r3{{103u}, 30u, {12.f, 0.f, 0.f}, {}, ObjectSimulationClass::Dynamic};
    if (!appendRecord(chunk, r1) || !appendRecord(chunk, r2) || !appendRecord(chunk, r3)) {
        return 3;
    }
    if (appendRecord(chunk, r3)) {
        return 4;
    }

    ChunkObjectRecord const* f = findRecordByGuid(chunk, {102u});
    if (f == nullptr || f->archetypeId != 20u || f->parent.guid != 101u) {
        return 5;
    }
    if (f->simulationClass != ObjectSimulationClass::Static) {
        return 10;
    }
    ChunkObjectRecord const* d = findRecordByGuid(chunk, {103u});
    if (d == nullptr || d->simulationClass != ObjectSimulationClass::Dynamic) {
        return 11;
    }
    if (findRecordByGuid(chunk, {999u}) != nullptr) {
        return 6;
    }

    std::array<WorldObjectRef, 2> out{};
    const std::size_t hits = queryRecordsInAabb(chunk, math::Aabb{{-1.f, -1.f, -1.f}, {10.f, 1.f, 1.f}}, out);
    if (hits != 2u) {
        return 7;
    }
    if (out[0].guid != 101u || out[1].guid != 102u) {
        return 8;
    }

    std::array<WorldObjectRef, 1> capped{};
    const std::size_t hitsCapped = queryRecordsInAabb(chunk, math::Aabb{{-1.f, -1.f, -1.f}, {20.f, 1.f, 1.f}}, capped);
    if (hitsCapped != 3u || capped[0].guid != 101u) {
        return 9;
    }

    return 0;
}
