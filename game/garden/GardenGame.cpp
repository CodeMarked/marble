#include "garden/GardenGame.hpp"

#include "garden/detail/GardenGameState.hpp"
#include "garden/GardenGameHelpers.hpp"
#include "shared/InitSampleMeshVulkanRhi.hpp"

#include "core/MeshAssetV1.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "core/Simulation.hpp"
#include "render/IRenderBackend.hpp"
#include "render/ProceduralMeshVulkan.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace marble::garden_app {

class RenderPhase final : public marble::core::Engine::IRenderPhase {
public:
    explicit RenderPhase(GardenGame::State* s) : state_(s) {}

    void render(marble::core::Engine::FrameContext const& ctx) override {
        if (state_) {
            state_->renderFrame(ctx);
        }
    }

private:
    GardenGame::State* state_;
};

GardenGame::GardenGame(marble::core::Engine& engine, GardenSessionKind session, RemoteClientParams clientParams)
    : engine_(engine), state_(std::make_unique<State>(engine, session, std::move(clientParams))) {}

GardenGame::~GardenGame() {
    if (state_) {
        state_->rhi.shutdown();
    }
    engine_.resetPhasesToDefaults();
}

void GardenGame::installPhases() {
    auto* raw = state_.get();
    auto rend = std::make_unique<RenderPhase>(raw);
    auto fixed = std::make_unique<marble::core::FixedStepSimulationPhase>();
    fixed->setStepCallback([raw](marble::core::Engine::FrameContext const& ctx) { raw->step(ctx); });
    engine_.setSimulationPhase(std::move(fixed));
    engine_.setRenderPhase(std::move(rend));
}

bool GardenGame::initGraphics(std::string shaderDirectory, std::optional<std::uint32_t> physicalDeviceIndex) {
    if (!engine_.window()) {
        return false;
    }
    if (!marble::game_shared::initSampleMeshVulkanRhiFromAssetsOrShaderDirectory(
            *engine_.window(),
            state_->rhi,
            state_->assetRegistry_,
            engine_.assetsRootPath(),
            std::filesystem::path(shaderDirectory),
            "Garden",
            physicalDeviceIndex)) {
        return false;
    }
    marble::render::IRenderBackend& rb = state_->rhi;
    rb.setClearColor(0.07f, 0.16f, 0.19f, 1.f);

    std::vector<marble::render::VulkanRhi::Vertex> terrVx;
    std::vector<std::uint32_t> terrIx;
    detail::fillTerrainMeshCpu(state_->layout.terrain, terrVx, terrIx);
    state_->meshTerrain = state_->rhi.uploadMesh(terrVx, terrIx);
    if (state_->meshTerrain == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<marble::render::VulkanRhi::Vertex> cv;
    std::vector<std::uint32_t> ci;
    marble::render::addCube(cv, ci, {1.f, 1.f, 1.f});
    state_->meshCube = state_->rhi.uploadMesh(cv, ci);
    if (state_->meshCube == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    using marble::garden::GardenPropMesh;
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Cube)] = state_->meshCube;

    std::vector<marble::render::VulkanRhi::Vertex> ov;
    std::vector<std::uint32_t> oi;
    marble::render::addOctahedron(ov, oi, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Octahedron)] = state_->rhi.uploadMesh(ov, oi);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Octahedron)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<marble::render::VulkanRhi::Vertex> iv;
    std::vector<std::uint32_t> ii;
    marble::render::addIcosahedronSubdividedClean(iv, ii, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Icosahedron)] = state_->rhi.uploadMesh(iv, ii);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Icosahedron)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Foliage)] =
        state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Icosahedron)];

    std::vector<marble::render::VulkanRhi::Vertex> dv;
    std::vector<std::uint32_t> di;
    marble::render::addDiscExtrudedY(dv, di, 20, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Disc)] = state_->rhi.uploadMesh(dv, di);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Disc)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<marble::render::VulkanRhi::Vertex> tv;
    std::vector<std::uint32_t> ti;
    // +Y cylinder in [-0.5,0.5] (same convention as TrunkY and garden capsule draw/physics).
    marble::render::addCylinderAlongY(tv, ti, 16, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TwigCapsule)] = state_->rhi.uploadMesh(tv, ti);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TwigCapsule)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<marble::render::VulkanRhi::Vertex> yv;
    std::vector<std::uint32_t> yi;
    marble::render::addCylinderAlongY(yv, yi, 14, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TrunkY)] = state_->rhi.uploadMesh(yv, yi);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TrunkY)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    {
        std::vector<std::uint8_t> const rampBytes = marble::core::meshAssetV1BuildGardenTiltedRampTemplateBytes();
        std::optional<marble::core::MeshAssetV1CpuViews> const rv =
            marble::core::meshAssetV1TryParse(std::span<std::uint8_t const>(rampBytes.data(), rampBytes.size()));
        if (!rv.has_value()) {
            return false;
        }
        state_->meshProps[static_cast<std::size_t>(GardenPropMesh::MeshTiltedRamp)] =
            state_->rhi.uploadMeshFromMeshAssetV1CpuViews(*rv);
        if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::MeshTiltedRamp)] ==
            std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
    }
    {
        std::vector<std::uint8_t> const benchBytes = marble::core::meshAssetV1BuildGardenBenchSlatsTemplateBytes();
        std::optional<marble::core::MeshAssetV1CpuViews> const bv =
            marble::core::meshAssetV1TryParse(std::span<std::uint8_t const>(benchBytes.data(), benchBytes.size()));
        if (!bv.has_value()) {
            return false;
        }
        state_->meshProps[static_cast<std::size_t>(GardenPropMesh::MeshBenchSlats)] =
            state_->rhi.uploadMeshFromMeshAssetV1CpuViews(*bv);
        if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::MeshBenchSlats)] ==
            std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
    }

    std::vector<marble::render::VulkanRhi::Vertex> sv;
    std::vector<std::uint32_t> si;
    marble::render::addUvSphere(sv, si, 1.f, 16, 24, {1.f, 1.f, 1.f});
    state_->meshSphere = state_->rhi.uploadMesh(sv, si);
    if (state_->meshSphere == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    return true;
}

} // namespace marble::garden_app
