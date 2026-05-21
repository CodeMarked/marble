#include "garden/GardenPropContracts.hpp"
#include "garden/GardenSimulation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "math/Mat4.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsIntegration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <span>

namespace {

using marble::gameplay::SimulationIsland;
using marble::gameplay::localGameplayAabbToJolt;
using marble::gameplay::localGameplayHeightFieldToJolt;
using marble::gameplay::localGameplayToJolt;
using marble::garden::GardenColliderKind;
using marble::garden::GardenLayout;
using marble::garden::GardenPropMesh;
using marble::garden::GardenStaticPhysicsProxy;
using marble::garden::buildGardenLayout;
using marble::garden::gardenAddStaticPropBodiesFromLayout;
using marble::garden::gardenBallOnGround;
using marble::garden::gardenCapsuleMaxTerrainClearanceDeficit;
using marble::garden::gardenJumpImpulseFromHoldSeconds;
using marble::garden::gardenSnapCapsuleCenterOnAnalyticTerrain;
using marble::garden::gardenStaticLayoutPhysicsMatchesContract;
using marble::garden::gardenTerrainHeightAt;
using marble::garden::gardenTerrainHeightLayoutMeters;
using marble::garden::gardenTerrainHeightRuntimeBias;
using marble::garden::kGardenLayoutTerrainStaticRestBiasM;
using marble::garden::kGardenBallOnGroundClearanceM;
using marble::garden::kGardenBallOnGroundFootprintScale;
using marble::garden::kGardenBallOnGroundMaxUpwardVyMps;
using marble::garden::kGardenJumpImpulseMax;
using marble::garden::kGardenJumpImpulseMin;
using marble::garden::kGardenRadius;
using marble::garden::kMarbleRadius;
using marble::garden::placeMarblesInArena;
using marble::garden::rayHitsSphere;
using marble::garden::rayIntersectHorizontalPlane;
using marble::math::Vec3;
using marble::physics::PhysicsBodyMaterial;
using marble::physics::PhysicsCylindricalXZClamp;
using marble::physics::PhysicsDynamicSphereDesc;
using marble::physics::PhysicsStaticHeightFieldDesc;
using marble::physics::PhysicsStaticBoxDesc;
using marble::physics::PhysicsStepOptions;
using marble::physics::RigidBodyKinematics;
using marble::physics::SimplePhysicsWorld;
using marble::physics::createJoltPhysicsScene;

void rebuildGardenPhysicsScene(
    marble::physics::IPhysicsScene& scene,
    GardenLayout const& layout,
    std::array<RigidBodyKinematics, 2>& marbles,
    std::array<marble::physics::PhysicsBodyId, 2>& marbleBodyIds,
    SimulationIsland const& island
) {
    scene.clear();
    PhysicsBodyMaterial staticMat{};
    staticMat.restitution = 0.30f;
    staticMat.friction = 0.55f;
    auto const& tr = layout.terrain;
    if (tr.sampleCount >= 2u && tr.heights.size() == static_cast<std::size_t>(tr.sampleCount) * tr.sampleCount) {
        PhysicsStaticHeightFieldDesc hf{};
        hf.offset = tr.origin;
        hf.scale = {tr.cellSize, 1.f, tr.cellSize};
        hf.sampleCount = tr.sampleCount;
        hf.heights = std::span<float const>(tr.heights.data(), tr.heights.size());
        hf.material = staticMat;
        PhysicsStaticHeightFieldDesc const hfJ = localGameplayHeightFieldToJolt(island, hf);
        (void)scene.addStaticHeightField(hfJ);
    }
    gardenAddStaticPropBodiesFromLayout(scene, layout, island, staticMat);
    PhysicsBodyMaterial marbleMat{};
    marbleMat.restitution = 0.672f;
    marbleMat.friction = 0.42f;
    marbleMat.linearDamping = 0.02f;
    marbleMat.angularDamping = 0.10f;
    for (std::size_t i = 0; i < marbles.size(); ++i) {
        PhysicsDynamicSphereDesc sd{};
        sd.center = localGameplayToJolt(island, marbles[i].position);
        sd.linearVelocity = marbles[i].linearVelocity;
        sd.radius = kMarbleRadius;
        sd.invMass = marbles[i].invMass;
        sd.enhancedInternalEdgeRemoval = true;
        sd.material = marbleMat;
        marbleBodyIds[i] = scene.addDynamicSphere(sd);
    }
    scene.optimizeBroadPhase();
}

} // namespace

int main() {
    GardenLayout layout{};
    buildGardenLayout(42u, layout);
    if (layout.terrain.sampleCount < 2u ||
        layout.terrain.heights.size() != static_cast<std::size_t>(layout.terrain.sampleCount) * layout.terrain.sampleCount) {
        return 7;
    }
    GardenLayout layoutDup{};
    buildGardenLayout(42u, layoutDup);
    if (layout.terrain.heights != layoutDup.terrain.heights) {
        return 8;
    }
    GardenLayout layoutOther{};
    buildGardenLayout(99991u, layoutOther);
    bool heightsDiffer = false;
    for (std::size_t i = 0; i < layout.terrain.heights.size(); i += 23) {
        if (std::fabs(layout.terrain.heights[i] - layoutOther.terrain.heights[i]) > 1e-3f) {
            heightsDiffer = true;
            break;
        }
    }
    if (!heightsDiffer) {
        return 9;
    }
    if (layout.staticColliders.empty() || layout.staticColliders.size() != layout.colliderTangentRetention.size() ||
        layout.kinds.size() != layout.staticColliders.size() ||
        layout.propMesh.size() != layout.staticColliders.size() ||
        layout.propYaw.size() != layout.staticColliders.size() ||
        layout.propCollisionMeshBytes.size() != layout.staticColliders.size()) {
        return 1;
    }

    {
        std::array<RigidBodyKinematics, 2> marblesGround{};
        placeMarblesInArena(marblesGround, layout);
        if (!gardenBallOnGround(layout, marblesGround[0], kMarbleRadius)) {
            return 12;
        }
        RigidBodyKinematics air = marblesGround[0];
        air.position.y += 50.f;
        if (gardenBallOnGround(layout, air, kMarbleRadius)) {
            return 13;
        }
        float const impShort = gardenJumpImpulseFromHoldSeconds(0.02f);
        if (impShort < kGardenJumpImpulseMin || impShort > kGardenJumpImpulseMax) {
            return 14;
        }
        float const impLong = gardenJumpImpulseFromHoldSeconds(100.f);
        if (impLong < kGardenJumpImpulseMax * 0.99f || impLong > kGardenJumpImpulseMax * 1.01f) {
            return 15;
        }
        RigidBodyKinematics rising = marblesGround[0];
        rising.linearVelocity.y = kGardenBallOnGroundMaxUpwardVyMps + 0.05f;
        if (gardenBallOnGround(layout, rising, kMarbleRadius)) {
            return 16;
        }
    }

    {
        // Synthetic tilted heightfield: footprint max terrain exceeds center-only height on +X slope.
        GardenLayout ramp{};
        ramp.terrain.sampleCount = 16u;
        ramp.terrain.cellSize = 1.f;
        ramp.terrain.origin = {0.f, 0.f, 0.f};
        ramp.terrain.heights.resize(256u);
        for (std::uint32_t iz = 0u; iz < 16u; ++iz) {
            for (std::uint32_t ix = 0u; ix < 16u; ++ix) {
                ramp.terrain.heights[static_cast<std::size_t>(iz) * 16u + ix] = 0.08f * static_cast<float>(ix);
            }
        }
        ramp.terrain.minHeight = 0.f;
        ramp.terrain.maxHeight = 0.08f * 15.f;

        float const px = 8.3f;
        float const pz = 8.0f;
        float const tyCenter = gardenTerrainHeightAt(ramp.terrain, px, pz);
        float const d = kMarbleRadius * kGardenBallOnGroundFootprintScale;
        float const tyEast = gardenTerrainHeightAt(ramp.terrain, px + d, pz);
        if (!(tyEast > tyCenter + 1e-4f)) {
            return 17;
        }
        float tyMax = tyCenter;
        tyMax = std::max(tyMax, gardenTerrainHeightAt(ramp.terrain, px + d, pz));
        tyMax = std::max(tyMax, gardenTerrainHeightAt(ramp.terrain, px - d, pz));
        tyMax = std::max(tyMax, gardenTerrainHeightAt(ramp.terrain, px, pz + d));
        tyMax = std::max(tyMax, gardenTerrainHeightAt(ramp.terrain, px, pz - d));
        float const bottomOk = tyMax + kGardenBallOnGroundClearanceM - 0.02f;
        RigidBodyKinematics onRamp{};
        onRamp.invMass = 1.f;
        onRamp.position = {px, bottomOk + kMarbleRadius, pz};
        onRamp.linearVelocity = {0.f, 0.f, 0.f};
        if (!gardenBallOnGround(ramp, onRamp, kMarbleRadius)) {
            return 18;
        }
        float const bottomTooHigh = tyMax + kGardenBallOnGroundClearanceM + 0.5f;
        onRamp.position.y = bottomTooHigh + kMarbleRadius;
        if (gardenBallOnGround(ramp, onRamp, kMarbleRadius)) {
            return 19;
        }
    }

    {
        // Sloped analytic terrain: oriented capsule snap clears heightfield at surface samples (+ clearance).
        GardenLayout capLayout{};
        capLayout.terrain.sampleCount = 16u;
        capLayout.terrain.cellSize = 1.f;
        capLayout.terrain.origin = {0.f, 0.f, 0.f};
        capLayout.terrain.heights.resize(256u);
        for (std::uint32_t iz = 0u; iz < 16u; ++iz) {
            for (std::uint32_t ix = 0u; ix < 16u; ++ix) {
                capLayout.terrain.heights[static_cast<std::size_t>(iz) * 16u + ix] = 0.06f * static_cast<float>(ix);
            }
        }
        capLayout.terrain.minHeight = 0.f;
        capLayout.terrain.maxHeight = 0.06f * 15.f;

        float const hh = 1.1f;
        float const r = 0.14f;
        Vec3 c{7.4f, 9.f, 7.6f};
        float const yaw = 0.7f;
        float const pitch = 0.5f;
        float const roll = 0.33f;
        std::uint8_t const axis = 1u;
        float const clearance = 0.004f;
        gardenSnapCapsuleCenterOnAnalyticTerrain(capLayout.terrain, c, hh, r, yaw, pitch, roll, axis, clearance);

        marble::math::Mat4 const rot =
            marble::math::Mat4::rotationY(yaw) * marble::math::Mat4::rotationX(pitch) * marble::math::Mat4::rotationZ(roll);
        Vec3 const meshAxis{1.f, 0.f, 0.f};
        Vec3 const wAxis = marble::math::transformDirection(rot, meshAxis);
        float const deficit =
            gardenCapsuleMaxTerrainClearanceDeficit(capLayout.terrain, c, wAxis, hh, r, clearance);
        if (deficit > 5.0e-4f) {
            return 22;
        }
    }

    {
        // Flat synthetic terrain: resting on surface is grounded; spawn clearance constant matches header.
        GardenLayout flat{};
        flat.terrain.sampleCount = 4u;
        flat.terrain.cellSize = 2.f;
        flat.terrain.origin = {0.f, 0.f, 0.f};
        flat.terrain.heights.assign(16u, 5.f);
        flat.terrain.minHeight = 5.f;
        flat.terrain.maxHeight = 5.f;
        float const ty = gardenTerrainHeightAt(flat.terrain, 3.f, 3.f);
        RigidBodyKinematics m{};
        m.invMass = 1.f;
        m.position = {3.f, ty + kMarbleRadius + 0.01f, 3.f};
        m.linearVelocity = {};
        if (!gardenBallOnGround(flat, m, kMarbleRadius)) {
            return 20;
        }
        m.position.y = ty + kMarbleRadius + kGardenBallOnGroundClearanceM + 0.4f;
        if (gardenBallOnGround(flat, m, kMarbleRadius)) {
            return 21;
        }
    }

    {
        // Procedural layout terrain: bilinear height stays within analytic min/max band; static placement is layout + bias only.
        constexpr float kHfBandEps = 5.0e-4f;
        auto const& tr = layout.terrain;
        for (std::uint32_t iz = 0u; iz < tr.sampleCount; iz += 9u) {
            for (std::uint32_t ix = 0u; ix < tr.sampleCount; ix += 9u) {
                float const wx = tr.origin.x + static_cast<float>(ix) * tr.cellSize;
                float const wz = tr.origin.z + static_cast<float>(iz) * tr.cellSize;
                float const hLay = gardenTerrainHeightLayoutMeters(tr, wx, wz);
                float const hRt = gardenTerrainHeightRuntimeBias(tr, wx, wz);
                if (hLay < tr.minHeight - kHfBandEps || hLay > tr.maxHeight + kHfBandEps) {
                    return 23;
                }
                if (std::fabs(hRt - (hLay + kGardenLayoutTerrainStaticRestBiasM)) > 1.0e-5f) {
                    return 24;
                }
            }
        }
    }
    {
        // Along +X on a synthetic ramp, sampled layout height is monotone (no local decreases off grid edges).
        GardenLayout slope{};
        constexpr std::uint32_t Sn = 32u;
        slope.terrain.sampleCount = Sn;
        slope.terrain.cellSize = 1.f;
        slope.terrain.origin = {0.f, 0.f, 0.f};
        slope.terrain.heights.resize(static_cast<std::size_t>(Sn) * Sn);
        for (std::uint32_t iz = 0u; iz < Sn; ++iz) {
            for (std::uint32_t ix = 0u; ix < Sn; ++ix) {
                slope.terrain.heights[static_cast<std::size_t>(iz) * Sn + ix] = 0.05f * static_cast<float>(ix);
            }
        }
        slope.terrain.minHeight = 0.f;
        slope.terrain.maxHeight = 0.05f * static_cast<float>(Sn - 1u);
        float prev = gardenTerrainHeightLayoutMeters(slope.terrain, 0.75f, 9.25f);
        for (int s = 1; s < 45; ++s) {
            float const wx = 0.75f + 0.42f * static_cast<float>(s);
            float const cur = gardenTerrainHeightLayoutMeters(slope.terrain, wx, 9.25f);
            if (cur + 1.0e-5f < prev) {
                return 25;
            }
            prev = cur;
        }
    }
    {
        // [`buildGardenLayout`] emitted statics match proxy/mesh contracts ([`GardenPropContracts.hpp`]).
        for (std::size_t i = 0; i < layout.kinds.size(); ++i) {
            GardenStaticPhysicsProxy const& px =
                i < layout.staticPhysicsProxies.size() ? layout.staticPhysicsProxies[i] : GardenStaticPhysicsProxy{};
            auto const mesh = static_cast<GardenPropMesh>(i < layout.propMesh.size() ? layout.propMesh[i] : 0u);
            if (!gardenStaticLayoutPhysicsMatchesContract(layout.kinds[i], px, mesh)) {
                return 26;
            }
        }
    }
    {
        // Log-only settle regression (seed 42): trunk AABB center-Y extrema remain stable when settle math changes.
        constexpr float kGoldenLogCyMin = 2.269137f;
        constexpr float kGoldenLogCyMax = 6.084480f;
        constexpr float kGoldenSumY = 41.945747f;
        constexpr float kTolCy = 0.02f;
        constexpr float kTolSum = 0.04f;
        float minCy = 1.0e30f;
        float maxCy = -1.0e30f;
        double sumCy = 0.0;
        std::size_t logCount = 0u;
        for (std::size_t i = 0; i < layout.kinds.size(); ++i) {
            if (layout.kinds[i] != GardenColliderKind::Log) {
                continue;
            }
            auto const& b = layout.staticColliders[i];
            float const cy = 0.5f * (b.min.y + b.max.y);
            minCy = std::min(minCy, cy);
            maxCy = std::max(maxCy, cy);
            sumCy += static_cast<double>(cy);
            ++logCount;
        }
        if (logCount != 12u) {
            return 27;
        }
        if (std::fabs(minCy - kGoldenLogCyMin) > kTolCy || std::fabs(maxCy - kGoldenLogCyMax) > kTolCy ||
            std::fabs(static_cast<float>(sumCy) - kGoldenSumY) > kTolSum) {
            return 28;
        }
    }

    float t = 0.f;
    Vec3 rayO{0.f, 2.f, 3.f};
    Vec3 rayD{0.f, -0.5f, -1.f};
    if (!rayHitsSphere(rayO, rayD, Vec3::zero(), 1.f, t) || t < 0.f) {
        return 2;
    }

    Vec3 planeHit{};
    if (!rayIntersectHorizontalPlane(Vec3{0.f, 2.f, 0.f}, Vec3{0.3f, -1.f, 0.f}, 0.f, planeHit)) {
        return 5;
    }
    if (std::fabs(planeHit.y) > 1e-3f || std::fabs(planeHit.x - 0.6f) > 0.05f) {
        return 6;
    }

    SimplePhysicsWorld world{};
    world.setSettings({.gravity = {0.f, -9.81f, 0.f}, .maxSubSteps = 1u});
    constexpr float dt = 1.f / 60.f;
    float const minCenterY = layout.terrain.minHeight - kMarbleRadius - 0.55f;
    PhysicsCylindricalXZClamp const clamp{
        kGardenRadius - kMarbleRadius - 0.02f,
        minCenterY,
    };

    auto runWithIsland = [&](SimulationIsland const& island, std::array<RigidBodyKinematics, 2>& marblesOut) -> int {
        placeMarblesInArena(marblesOut, layout);
        auto scene = createJoltPhysicsScene();
        std::array<marble::physics::PhysicsBodyId, 2> marbleBodyIds{};
        rebuildGardenPhysicsScene(*scene, layout, marblesOut, marbleBodyIds, island);
        PhysicsStepOptions const stepOpts{&clamp, std::span(marbleBodyIds)};
        for (int i = 0; i < 480; ++i) {
            scene->syncHostVelocitiesBeforeStep(marbleBodyIds, marblesOut.data(), marblesOut.size());
            scene->step(dt, world.settings(), stepOpts);
            scene->readBackKinematics(marbleBodyIds, marblesOut.data(), marblesOut.size());
        }
        for (auto const& m : marblesOut) {
            float const xz = std::sqrt(m.position.x * m.position.x + m.position.z * m.position.z);
            if (xz > kGardenRadius + 0.05f) {
                return 3;
            }
            if (m.position.y < layout.terrain.minHeight - 20.f || m.position.y > layout.terrain.maxHeight + 70.f) {
                return 4;
            }
            if (m.position.x != m.position.x || m.linearVelocity.x != m.linearVelocity.x) {
                return 10;
            }
        }
        return 0;
    };

    std::array<RigidBodyKinematics, 2> marblesRef{};
    int const rc0 = runWithIsland(SimulationIsland{}, marblesRef);
    if (rc0 != 0) {
        return rc0;
    }

    std::array<RigidBodyKinematics, 2> marblesBig{};
    SimulationIsland const bigIsland{1.0e6, 0.0, 1.0e6};
    int const rc1 = runWithIsland(bigIsland, marblesBig);
    if (rc1 != 0) {
        return rc1;
    }

    for (std::size_t i = 0; i < marblesRef.size(); ++i) {
        float const dx = marblesRef[i].position.x - marblesBig[i].position.x;
        float const dy = marblesRef[i].position.y - marblesBig[i].position.y;
        float const dz = marblesRef[i].position.z - marblesBig[i].position.z;
        float const err = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (err > 0.05f) {
            return 11;
        }
    }

    return 0;
}
