#include "render/DrawOrder.hpp"
#include "render/MaterialId.hpp"
#include "render/RenderTypes.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace {

struct DrawRecord {
    std::uint32_t meshIndex{};
    std::uint8_t drawLayer{};
    std::uint8_t materialId{};
};

/// Mirrors what a recording `IRenderBackend` would observe from the submitted span (submission order).
inline void recordDrawBatch(std::span<marble::render::MeshDrawInstance const> draws, std::vector<DrawRecord>& out) {
    out.clear();
    out.reserve(draws.size());
    for (marble::render::MeshDrawInstance const& d : draws) {
        out.push_back(DrawRecord{d.meshIndex, d.drawLayer, d.materialId});
    }
}

[[nodiscard]] int checkSortLayers() {
    using marble::render::MeshDrawInstance;
    using marble::render::sortMeshDrawInstanceIndices;

    std::vector<MeshDrawInstance> draws(4);
    draws[0].drawLayer = 1;
    draws[0].meshIndex = 10;
    draws[1].drawLayer = 0;
    draws[1].meshIndex = 20;
    draws[2].drawLayer = 1;
    draws[2].meshIndex = 30;
    draws[3].drawLayer = 0;
    draws[3].meshIndex = 40;

    std::vector<std::uint32_t> idx(4);
    sortMeshDrawInstanceIndices({draws.data(), draws.size()}, {idx.data(), idx.size()});

    if (draws[idx[0]].drawLayer != 0 || draws[idx[1]].drawLayer != 0) {
        return 10;
    }
    if (draws[idx[2]].drawLayer != 1 || draws[idx[3]].drawLayer != 1) {
        return 11;
    }
    if (draws[idx[0]].meshIndex != 20 || draws[idx[1]].meshIndex != 40) {
        return 12;
    }
    if (draws[idx[2]].meshIndex != 10 || draws[idx[3]].meshIndex != 30) {
        return 13;
    }
    return 0;
}

[[nodiscard]] int checkSortMaterialThenMesh() {
    using marble::render::MeshDrawInstance;
    using marble::render::sortMeshDrawInstanceIndices;

    std::vector<MeshDrawInstance> draws(3);
    draws[0].drawLayer = 0;
    draws[0].materialId = 1;
    draws[0].meshIndex = 5;
    draws[1].drawLayer = 0;
    draws[1].materialId = 0;
    draws[1].meshIndex = 99;
    draws[2].drawLayer = 0;
    draws[2].materialId = 1;
    draws[2].meshIndex = 1;

    std::vector<std::uint32_t> idx(3);
    sortMeshDrawInstanceIndices({draws.data(), draws.size()}, {idx.data(), idx.size()});

    if (draws[idx[0]].materialId != 0) {
        return 20;
    }
    if (draws[idx[1]].materialId != 1 || draws[idx[1]].meshIndex != 1) {
        return 21;
    }
    if (draws[idx[2]].materialId != 1 || draws[idx[2]].meshIndex != 5) {
        return 22;
    }
    return 0;
}

[[nodiscard]] int checkRecordingMaterialPassthrough() {
    std::vector<marble::render::MeshDrawInstance> batch(2);
    batch[0].meshIndex = 7;
    batch[0].materialId = marble::render::kMaterialDefault;
    batch[1].meshIndex = 8;
    batch[1].materialId = marble::render::kMaterialTranslucent;

    std::vector<DrawRecord> records;
    recordDrawBatch({batch.data(), batch.size()}, records);

    if (records.size() != 2) {
        return 32;
    }
    if (records[0].meshIndex != 7 || records[0].materialId != marble::render::kMaterialDefault) {
        return 33;
    }
    if (records[1].meshIndex != 8 || records[1].materialId != marble::render::kMaterialTranslucent) {
        return 34;
    }
    return 0;
}

} // namespace

int main() {
    if (int e = checkSortLayers()) {
        return e;
    }
    if (int e = checkSortMaterialThenMesh()) {
        return e;
    }
    if (int e = checkRecordingMaterialPassthrough()) {
        return e;
    }
    return 0;
}
