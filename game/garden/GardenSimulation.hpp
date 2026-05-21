#pragma once

#include "math/Geometry.hpp"
#include "math/Vec3.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace marble::physics {
class IPhysicsScene;
}

namespace marble::gameplay {
struct SimulationIsland;
}

namespace marble::garden {

/// Human-scale reference (~standing height) for prop sizing comments and boulder scale.
inline constexpr float kHumanHeightReference = 1.8f;
/// Playable yard half-extent (~180 m across); cylindrical marble bounds use the same value as `kGardenRadius`.
inline constexpr float kYardHalfExtent = 90.f;
inline constexpr float kGardenRadius = kYardHalfExtent;
/// Central play circle where marbles spawn (~14 m diameter).
inline constexpr float kArenaRadius = 7.f;
/// FIFA Law 2 size 5: circumference 68–70 cm → radius ~0.109–0.112 m; nominal sphere proxy (meters).
inline constexpr float kMarbleRadius = 0.11f;
/// Extra clearance above bilinear terrain when placing marbles (m). Jolt heightfield can sit slightly above analytic
/// samples after quantization; static props rest on the mesh but are not in [`gardenTerrainHeightAt`].
inline constexpr float kGardenMarbleSpawnClearanceAboveTerrainM = 0.12f;
/// Small upward shift (m) when sampling analytic height for **static** rests (walls, scatter, trees) vs the same grid
/// exported to [`IPhysicsScene::addStaticHeightField`]. Keeps layout sit-points from clipping slightly below the mesh.
inline constexpr float kGardenLayoutTerrainStaticRestBiasM = 0.008f;
/// Per-instance vertical tuck into sampled terrain after analytic sit (meters; randomized in layout for a planted look).
inline constexpr float kGardenDecorGroundTuckMinM = 0.006f;
inline constexpr float kGardenDecorGroundTuckMaxM = 0.058f;
/// Clearance used in final decor sit clamps (tighter than legacy 4 mm so bury/tuck reads against the heightfield).
inline constexpr float kGardenDecorSitClearanceM = 0.0012f;
/// Reject jump gating when upward speed exceeds this (m/s) — treat as leaving the ground / airborne.
inline constexpr float kGardenBallOnGroundMaxUpwardVyMps = 0.9f;
/// Slack (m) from sphere bottom to max footprint terrain height: Jolt speculative contact, bilinear vs mesh, quantization.
inline constexpr float kGardenBallOnGroundClearanceM = 0.36f;
/// Footprint sample offset as a fraction of marble radius on ±X/±Z for [`gardenBallOnGround`] max terrain (slopes).
inline constexpr float kGardenBallOnGroundFootprintScale = 0.85f;
/// Regulation mass 410–450 g at kickoff; nominal (kg). Use `invMass = 1.f / kPlayerBallMassKg` with SI forces.
inline constexpr float kPlayerBallMassKg = 0.43f;
/// Jump charge cap and impulse range (matches local `GardenGame` lawn jump).
inline constexpr float kGardenJumpChargeMaxSec = 0.42f;
inline constexpr float kGardenJumpImpulseMin = 1.55f;
inline constexpr float kGardenJumpImpulseMax = 4.85f;
/// Horizontal roll input strength (impulse scale per second) for garden marbles — local, dedicated server, and client prediction.
inline constexpr float kGardenMarbleRollStrength = 4.6f;
/// Clamp for horizontal speed (m/s) after roll impulses.
inline constexpr float kGardenMarbleMaxHorizSpeed = 5.2f;
/// If a marble's center is farther than this from **every** other marble center, replication marks it
/// [`PhysicsSimulationTier::CruiseOrbit`] (prototype "bandwidth" tier); otherwise [`Contact`] ("engagement").
inline constexpr float kGardenNetSimNearOtherMarbleM = 4.f;
/// Heightfield samples per side (128² vertices). Must align with Jolt heightfield block packing (power-of-two friendly).
inline constexpr std::uint32_t kGardenTerrainSampleCount = 128u;
/// Default `garden_server --seed` / remote-client layout seed; keep in sync with [`RemoteClientParams::layoutSeed`](GardenGame.hpp).
inline constexpr std::uint32_t kGardenDedicatedServerDefaultLayoutSeed = 42u;

/// Procedural ground height samples for render + Jolt static heightfield (`IPhysicsScene::addStaticHeightField`).
/// Index `iz * sampleCount + ix` → world `(origin.x + ix * cellSize, height, origin.z + iz * cellSize)`.
struct GardenTerrain {
    std::uint32_t sampleCount{};
    float cellSize{};
    math::Vec3 origin{};
    std::vector<float> heights{};
    float minHeight{};
    float maxHeight{};
};

enum class GardenColliderKind : std::uint8_t {
    Ground,
    Wall,
    /// Small sandy undulation (legacy name kept for callers).
    GrassBump,
    /// Sandy pocket (higher drag); used inside scattered pits only.
    SandPit,
    /// Large immovable glacial boulder; always static (never runtime loose).
    Boulder,
    Rock,
    Leaf,
    /// Fallen branch / stick resting on rocks or logs (small cylindrical mesh).
    TwigScatter,
    Twig,
    /// Heavier fallen branch / log (TrunkY mesh + capsule; twigs use TwigCapsule).
    Log,
    /// Raised sandy stone terrace (same render family as ground).
    Terrace,
    /// Vertical jungle trunk (static).
    TreeTrunk,
    /// Canopy mass (static, approximate collision).
    TreeFoliage,
    /// MRBMESH1 hero props (ramp / bench); static only in current layouts.
    HeroPlayground,
};

/// Inverse mass for log bake / debris settle (wall bricks are very heavy; twigs light).
[[nodiscard]] inline float gardenLooseBodyInverseMass(GardenColliderKind k) noexcept {
    switch (k) {
    case GardenColliderKind::Wall:
        return 0.0018f;
    case GardenColliderKind::Boulder:
        return 0.f;
    case GardenColliderKind::Rock:
        return 0.032f;
    case GardenColliderKind::Log:
        return 0.028f;
    case GardenColliderKind::Twig:
        return 0.15f;
    case GardenColliderKind::HeroPlayground:
        return 0.f;
    default:
        return 0.05f;
    }
}

/// Visual mesh for a static prop. Indices match upload order in the garden app.
enum class GardenPropMesh : std::uint8_t {
    Cube = 0,
    Octahedron,
    Icosahedron,
    Disc,
    TwigCapsule,
    TrunkY,
    Foliage,
    MeshTiltedRamp,
    MeshBenchSlats,
    Count
};

/// Tighter Jolt proxy than world AABB for [`staticColliders`]; index-aligned with that vector.
enum class GardenStaticPhysicsProxyKind : std::uint8_t {
    AabbBox = 0,
    OrientedBox,
    Sphere,
    Capsule,
    /// MRBMESH1 bytes at [`GardenLayout::propCollisionMeshBytes`] for the same collider index → convex hull (resampled).
    ConvexHullFromMeshBytes,
    /// Same MRBMESH1 slot → indexed triangle mesh (CCW, +Y convention as in asset).
    TriangleMeshFromMeshBytes,
};

struct GardenStaticPhysicsProxy {
    GardenStaticPhysicsProxyKind kind{GardenStaticPhysicsProxyKind::AabbBox};
    float sphereRadius{};
    math::Vec3 orientedHalfExtents{};
    float orientedYaw{};
    float orientedPitch{};
    float orientedRoll{};
    float capsuleHalfHeight{};
    float capsuleRadius{};
    std::uint8_t intrinsicCylinderAxis{}; ///< 0 = body +Y, 1 = body +X, 2 = body +Z (see [`PhysicsStaticCapsuleDesc`]).
    float capsuleYaw{};
    float capsulePitch{};
    float capsuleRoll{};
};

/// Static scenery + collision: AABBs with optional tangential damping per box (simulates friction / grass vs rock).
struct GardenLayout {
    GardenTerrain terrain{};
    std::vector<math::Aabb> staticColliders{};
    /// Per-index physics proxy for Jolt (sphere / capsule / oriented box / MRBMESH1 hull or mesh / fallback AABB).
    std::vector<GardenStaticPhysicsProxy> staticPhysicsProxies{};
    std::vector<GardenColliderKind> kinds{};
    /// After a bounce, tangential velocity is scaled by this factor (lower = rougher).
    std::vector<float> colliderTangentRetention{};
    /// Per-instance render mesh and yaw about +Y (radians); same length as `staticColliders`.
    std::vector<std::uint8_t> propMesh{};
    std::vector<float> propYaw{};
    /// Tilt (radians); collider is the world AABB of the rotated `[-0.5,0.5]³` mesh box (same transform as draw).
    std::vector<float> propPitch{};
    std::vector<float> propRoll{};
    /// Optional MRBMESH1 v1 blobs for mesh-backed proxies (`ConvexHull*` / `TriangleMesh*`); index-aligned with `staticColliders`.
    std::vector<std::vector<std::uint8_t>> propCollisionMeshBytes{};
};

/// Deterministic procedural layout from `seed`: acre-scale yard, coarse terrain, static boulders, **logs-only**
/// high-substep AABB settle (then frozen), then static small debris. Runtime marble motion uses the engine
/// [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp) (Jolt implementation; see [`ADR-0058`](../../docs/decisions/ADR-0058-physics-middleware-integration.md)).
///
/// **Terrain direction:** heightfield-first — uneven ground should become a sampled height grid + static heightfield
/// collider in the physics backend; keep full triangle meshes for props, overhangs, and hero pieces only.
///
/// Prop meshes use unit `[-0.5,0.5]³` space (see `GardenGame::initGraphics`). `staticColliders` are the world AABB of
/// the oriented unit box for overlap; the renderer scales by [`GardenStaticPhysicsProxy::orientedHalfExtents`] when
/// the proxy is [`OrientedBox`] or [`TriangleMeshFromMeshBytes`] so draw matches bake / Jolt.
void buildGardenLayout(std::uint32_t seed, GardenLayout& out) noexcept;

/// Append static prop bodies (not heightfield) from [`GardenLayout::staticPhysicsProxies`].
void gardenAddStaticPropBodiesFromLayout(
    marble::physics::IPhysicsScene& scene,
    GardenLayout const& layout,
    marble::gameplay::SimulationIsland const& island,
    marble::physics::PhysicsBodyMaterial const& propMat) noexcept;

/// Bilinear height on `terrain` at world XZ (clamped to the grid).
[[nodiscard]] float gardenTerrainHeightAt(GardenTerrain const& terrain, float worldX, float worldZ) noexcept;

/// Same bilinear sampling as [`gardenTerrainHeightAt`], documented as layout / analytic authority (SI meters).
[[nodiscard]] inline float gardenTerrainHeightLayoutMeters(GardenTerrain const& terrain, float worldX, float worldZ) noexcept {
    return gardenTerrainHeightAt(terrain, worldX, worldZ);
}

/// Layout analytic height plus the static rest bias (aligns sit points with exported heightfield / Jolt mesh); not for
/// marbles-in-motion (see [`kGardenMarbleSpawnClearanceAboveTerrainM`] for dynamic clearance).
[[nodiscard]] inline float gardenTerrainHeightRuntimeBias(GardenTerrain const& terrain, float worldX, float worldZ) noexcept {
    return gardenTerrainHeightLayoutMeters(terrain, worldX, worldZ) + kGardenLayoutTerrainStaticRestBiasM;
}

/// Same as [`gardenTerrainHeightRuntimeBias`] — use for wall/scatter/tree sit passes that must share one bias policy.
[[nodiscard]] inline float gardenTerrainHeightForStaticPlacement(
    GardenTerrain const& terrain, float worldX, float worldZ) noexcept {
    return gardenTerrainHeightRuntimeBias(terrain, worldX, worldZ);
}

/// Approximate terrain slope magnitude `sqrt((∂h/∂x)² + (∂h/∂z)²)` at world XZ (finite differences).
[[nodiscard]] float gardenTerrainSlopeMagnitude(GardenTerrain const& terrain, float worldX, float worldZ) noexcept;

/// Unit **outward ground normal** from the analytic heightfield (same finite-difference step as slope magnitude).
[[nodiscard]] math::Vec3 gardenTerrainUpNormal(GardenTerrain const& terrain, float worldX, float worldZ) noexcept;

/// Max terrain height under the XZ footprint of `foot` (corners, edge mids, center); same sampling as prop sit logic.
[[nodiscard]] float gardenMaxTerrainHeightUnderFootprint(GardenTerrain const& terrain, math::Aabb const& foot) noexcept;

/// Lowest world Y on an oriented capsule surface (`Ry·Rx·Rz` on intrinsic axis: 0 = +Y, 1 = +X, 2 = +Z), Jolt/render
/// convention. `worldAxisUnnormalized` is the transformed mesh cylinder axis (normalized internally).
[[nodiscard]] float gardenCapsuleLowestWorldY(
    math::Vec3 center,
    math::Vec3 worldAxisUnnormalized,
    float halfHeight,
    float radius
) noexcept;

/// Max `(terrain + clearance) - surfaceY` over the same capsule surface samples as [`gardenCapsuleLowestWorldY`]; 0 when clear.
[[nodiscard]] float gardenCapsuleMaxTerrainClearanceDeficit(
    GardenTerrain const& terrain,
    math::Vec3 center,
    math::Vec3 worldAxisUnnormalized,
    float halfHeight,
    float radius,
    float clearanceM
) noexcept;

/// Iteratively adjusts `center.y` so sampled capsule surface points clear the analytic heightfield (+ clearance).
void gardenSnapCapsuleCenterOnAnalyticTerrain(
    GardenTerrain const& terrain,
    math::Vec3& center,
    float halfHeight,
    float radius,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    std::uint8_t intrinsicCylinderAxis,
    float clearanceM
) noexcept;

/// Penetration lift then gentle sink so logs match terrain samples (used after snap / settle; avoids footprint hover).
void gardenCapsuleRelaxCenterYOnTerrain(
    GardenTerrain const& terrain,
    math::Vec3& center,
    float halfHeight,
    float radius,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    std::uint8_t intrinsicCylinderAxis,
    float clearanceM
) noexcept;

/// Re-run **log-only** gravity settle vs all non-log colliders (e.g. after edits). `buildGardenLayout` runs this once
/// after logs are spawned and before small static debris is added.
void settleGardenDebrisInPlace(GardenLayout& layout) noexcept;

/// Default starting poses: two marbles opposite in the arena, resting on the heightfield.
void placeMarblesInArena(std::array<physics::RigidBodyKinematics, 2>& marbles, GardenLayout const& layout) noexcept;

/// Rough grounded test vs heightfield only (props invisible here; good enough for jump gating). Uses max terrain under a
/// small XZ footprint so slopes match Jolt resting better than center-only sampling.
[[nodiscard]] bool gardenBallOnGround(
    GardenLayout const& layout,
    physics::RigidBodyKinematics const& ball,
    float radius
) noexcept;

/// Impulse magnitude (N·s style; scaled by `invMass` in [`applyImpulseLinear`]) from hold duration — same curve as local garden.
[[nodiscard]] float gardenJumpImpulseFromHoldSeconds(float holdSeconds) noexcept;

/// Ray vs solid sphere; `rayDir` need not be unit. Returns true and sets `outT` (distance along ray) if hit with t >= 0.
[[nodiscard]] bool rayHitsSphere(
    math::Vec3 rayOrigin,
    math::Vec3 rayDir,
    math::Vec3 sphereCenter,
    float sphereRadius,
    float& outT
) noexcept;

/// Ray vs horizontal plane `y = planeY`. `rayDir` need not be unit. Returns true if intersection with `t >= 0`.
[[nodiscard]] bool rayIntersectHorizontalPlane(
    math::Vec3 rayOrigin,
    math::Vec3 rayDir,
    float planeY,
    math::Vec3& outPoint
) noexcept; // t >= 0 along ray; fails if ray parallel to plane

} // namespace marble::garden
