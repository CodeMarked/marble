#include "garden/detail/GardenGameState.hpp"
#include "garden/GardenGameHelpers.hpp"
#include "garden/GardenRemotePresentation.hpp"
#include "garden/GardenSimulation.hpp"
#include "platform/window/Window.hpp"

#include "math/CameraLhVulkan.hpp"

#include <cstdio>
#include <span>

namespace marble::garden_app {

using namespace marble::gameplay;
using namespace marble::physics;
using namespace marble::garden;
using namespace marble::render;
using namespace detail;

void GardenGame::State::renderFrame(marble::core::Engine::FrameContext const& ctx) {
    (void)ctx;
    IRenderBackend& backend = rhi;
    if (!backend.initialized() || meshTerrain == std::numeric_limits<std::uint32_t>::max() ||
        meshCube == std::numeric_limits<std::uint32_t>::max() ||
        meshSphere == std::numeric_limits<std::uint32_t>::max()) {
        return;
    }
    for (std::uint32_t pm : meshProps) {
        if (pm == std::numeric_limits<std::uint32_t>::max()) {
            return;
        }
    }

    if (sessionKind_ == GardenSessionKind::RemoteClient && clientSession_.state() == ConnectionState::Connected &&
        clientSession_.hasServerTimeSync() && snapInterp_.frameCount() > 0u) {
        std::size_t const localSlot = localViewMarbleIndex();
        gardenRemoteRunInterpolationPhase(
            remoteNetTuning_,
            clientSession_,
            snapInterp_,
            kGardenRemoteServerSimulationHz,
            std::span<RigidBodyKinematics>(marbles.data(), marbleCount_),
            marbleCount_,
            std::span<float>(remoteDisplayYaw_.data(), remoteDisplayYaw_.size()),
            localSlot,
            remoteInterpDelaySmoothed_,
            std::span<marble::gameplay::PhysicsSimulationTier>(
                remoteMarbleReplTier_.data(), remoteMarbleReplTier_.size()),
            lastInterpLogTime_);
    }

    if (sessionKind_ == GardenSessionKind::RemoteClient && clientSession_.state() == ConnectionState::Connected) {
        std::size_t const localSlot = localViewMarbleIndex();
        gardenRemoteRunPredictReconcileAccum(
            remoteNetTuning_,
            static_cast<float>(ctx.deltaSeconds),
            1.f / kGardenRemoteServerSimulationHz,
            remoteClientPredictAccum_,
            localSlot,
            layout,
            pendingMoveX_,
            pendingMoveZ_,
            pendingButtons_,
            jumpWasHeld_,
            jumpChargeSec_,
            std::span<RigidBodyKinematics>(marbles.data(), marbleCount_),
            std::span<Vec3 const>(remoteAuthMarblePos_.data(), remoteAuthMarblePos_.size()),
            std::span<bool const>(remoteAuthMarbleValid_.data(), remoteAuthMarbleValid_.size()),
            clientSession_,
            sessionKind_ == GardenSessionKind::RemoteClient && snapInterp_.frameCount() > 0u ? &snapInterp_ : nullptr,
            kGardenRemoteServerSimulationHz,
            lastAckedServerSimTick_,
            desyncPosSnapCount_,
            lastDesyncLogTime_);
    }

    int fbW = 1, fbH = 1;
    if (auto* w = engine.window()) {
        w->getFramebufferSize(&fbW, &fbH);
    }
    float const aspect = static_cast<float>(fbW) / static_cast<float>(std::max(1, fbH));
    constexpr float fovy = 60.f * 3.14159265f / 180.f;

    std::size_t const viewMarble = localViewMarbleIndex();
    Vec3 const player = marbles[viewMarble].position;
    float const sx = std::sin(camYaw);
    float const cz = std::cos(camYaw);
    Vec3 const eye = player + Vec3{sx * camDist, camHeight, cz * camDist};
    Vec3 const target = player + Vec3{0.f, 0.06f, 0.f};
    Mat4 const view = marble::math::lookAtLh(eye, target, Vec3::unitY());
    Mat4 const proj = marble::math::perspectiveVulkan(fovy, aspect, 0.12f, 420.f);
    Mat4 const viewProj = proj * view;

    drawScratch.clear();
    drawScratch.reserve(layout.staticColliders.size() + 16);

    auto pushMesh = [&](std::uint32_t meshIdx, Mat4 const& model, Vec3 color) {
        MeshDrawInstance d{};
        d.meshIndex = meshIdx;
        d.model = model;
        d.color = color;
        drawScratch.push_back(d);
    };

    pushMesh(meshTerrain, Mat4::identity(), Vec3{1.f, 1.f, 1.f});

    for (std::size_t i = 0; i < layout.staticColliders.size(); ++i) {
        Aabb const& b = layout.staticColliders[i];
        Vec3 const ctr = (b.min + b.max) * 0.5f;
        Vec3 const ext = (b.max - b.min) * 0.5f;
        GardenStaticPhysicsProxy const& px =
            i < layout.staticPhysicsProxies.size() ? layout.staticPhysicsProxies[i] : GardenStaticPhysicsProxy{};
        GardenColliderKind const kind =
            i < layout.kinds.size() ? layout.kinds[i] : GardenColliderKind::Rock;
        Vec3 const col = kind == GardenColliderKind::Wall ? colorForWallBrick(b)
                                                         : colorForKind(kind, static_cast<std::uint32_t>(i));
        std::uint8_t mid = (i < layout.propMesh.size()) ? layout.propMesh[i] : 0u;
        if (mid >= static_cast<std::uint8_t>(kPropMeshCount)) {
            mid = 0u;
        }
        float const yawR = (i < layout.propYaw.size()) ? layout.propYaw[i] : 0.f;
        float const pitchR = (i < layout.propPitch.size()) ? layout.propPitch[i] : 0.f;
        float const rollR = (i < layout.propRoll.size()) ? layout.propRoll[i] : 0.f;
        Mat4 const rot = Mat4::rotationY(yawR) * Mat4::rotationX(pitchR) * Mat4::rotationZ(rollR);
        Mat4 model{};
        if (px.kind == GardenStaticPhysicsProxyKind::Capsule) {
            // TwigCapsule / TrunkY: unit cylinder along +Y (radius 0.5, y in [-0.5,0.5]); match Jolt capsule scale
            // and intrinsic axis (see [`twigOrLogCapsuleProxy`], [`JoltPhysicsScene::addStaticCapsule`]).
            Mat4 align = Mat4::identity();
            if (px.intrinsicCylinderAxis == 1u) {
                align = Mat4::rotationZ(1.5707963f);
            } else if (px.intrinsicCylinderAxis == 2u) {
                align = Mat4::rotationX(-1.5707963f);
            }
            float const rad = px.capsuleRadius > 1.0e-4f ? px.capsuleRadius : 1.0e-4f;
            float const hh = px.capsuleHalfHeight > 1.0e-4f ? px.capsuleHalfHeight : 1.0e-4f;
            Vec3 const s{2.f * rad, 2.f * hh, 2.f * rad};
            model = Mat4::translation(ctr) * rot * align * Mat4::scaling(s);
        } else if (px.kind == GardenStaticPhysicsProxyKind::Sphere) {
            // Layout `staticColliders` is often the world AABB of a yawed OBB; drawing with `ext` warps the mesh and
            // floats above the analytic sphere sit used in layout. Match the Jolt/static sphere proxy instead.
            float const r = px.sphereRadius > 1.0e-4f ? px.sphereRadius : 1.0e-4f;
            float const s = 2.f * r;
            model = Mat4::translation(ctr) * rot * Mat4::scaling(Vec3{s, s, s});
        } else {
            // Unit mesh in [-0.5,0.5]³. Rotated props: scale by body half-extents (matches bake / oriented box), not
            // by world-axis AABB half `ext` (which warps silhouettes and misaligns mesh collision).
            Vec3 const halfForDraw =
                (px.kind == GardenStaticPhysicsProxyKind::OrientedBox ||
                 px.kind == GardenStaticPhysicsProxyKind::TriangleMeshFromMeshBytes)
                    ? px.orientedHalfExtents
                    : ext;
            model = Mat4::translation(ctr) * rot * Mat4::scaling(halfForDraw * 2.f);
        }
        pushMesh(meshProps[mid], model, col);
    }

    for (std::size_t mi = 0; mi < marbleCount_; ++mi) {
        MeshDrawInstance d{};
        Vec3 col = mi == localViewMarbleIndex() ? Vec3{0.52f, 0.78f, 0.95f} : Vec3{0.82f, 0.88f, 0.92f};
        if (sessionKind_ == GardenSessionKind::RemoteClient && mi < remoteMarbleReplTier_.size() &&
            mi != localViewMarbleIndex() &&
            remoteMarbleReplTier_[mi] == marble::gameplay::PhysicsSimulationTier::CruiseOrbit) {
            // Warmer tint for prototype "bandwidth" tier (far from other marbles on authority).
            col = Vec3{0.92f, 0.72f, 0.38f};
        }
        d.meshIndex = meshSphere;
        Mat4 model = Mat4::translation(marbles[mi].position);
        if (mi < remoteAuthMarbleYawValid_.size() && remoteAuthMarbleYawValid_[mi]) {
            model = model * Mat4::rotationY(remoteDisplayYaw_[mi]);
        }
        d.model = model * Mat4::scaling({kMarbleRadius, kMarbleRadius, kMarbleRadius});
        d.color = col;
        d.drawFlags = marble::render::kPcFlagMeshEmissive;
        if (mi == localViewMarbleIndex()) {
            d.materialId = kMaterialTranslucent;
            d.drawLayer = 1;
            d.colorAlpha = 0.55f;
        }
        drawScratch.push_back(d);
    }

    if (auto* w = engine.window()) {
        FrameOverlayTint tint{};
        FrameOverlayTint const* overlay = nullptr;
        if (paused) {
            tint = FrameOverlayTint{0.02f, 0.025f, 0.07f, 0.42f};
            overlay = &tint;
        }
        (void)backend.drawFrame(*w, viewProj, drawScratch, overlay);
    }
}
} // namespace marble::garden_app
