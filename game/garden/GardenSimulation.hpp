#pragma once

#include "math/Geometry.hpp"
#include "math/Vec3.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <array>
#include <cstdint>
#include <vector>

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
/// Regulation mass 410–450 g at kickoff; nominal (kg). Use `invMass = 1.f / kPlayerBallMassKg` with SI forces.
inline constexpr float kPlayerBallMassKg = 0.43f;
/// Jump charge cap and impulse range (matches local `GardenGame` lawn jump).
inline constexpr float kGardenJumpChargeMaxSec = 0.42f;
inline constexpr float kGardenJumpImpulseMin = 1.55f;
inline constexpr float kGardenJumpImpulseMax = 4.85f;
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
    /// Heavier fallen branch / log (same TwigCapsule mesh, larger AABB).
    Log,
    /// Raised sandy stone terrace (same render family as ground).
    Terrace,
    /// Vertical jungle trunk (static).
    TreeTrunk,
    /// Canopy mass (static, approximate collision).
    TreeFoliage,
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
    Count
};

/// Static scenery + collision: AABBs with optional tangential damping per box (simulates friction / grass vs rock).
struct GardenLayout {
    GardenTerrain terrain{};
    std::vector<math::Aabb> staticColliders{};
    std::vector<GardenColliderKind> kinds{};
    /// After a bounce, tangential velocity is scaled by this factor (lower = rougher).
    std::vector<float> colliderTangentRetention{};
    /// Per-instance render mesh and yaw about +Y (radians); same length as `staticColliders`.
    std::vector<std::uint8_t> propMesh{};
    std::vector<float> propYaw{};
    /// Tilt (radians); collider is the world AABB of the rotated `[-0.5,0.5]³` mesh box (same transform as draw).
    std::vector<float> propPitch{};
    std::vector<float> propRoll{};
};

/// Deterministic procedural layout from `seed`: acre-scale yard, coarse terrain, static boulders, **logs-only**
/// high-substep AABB settle (then frozen), then static small debris. Runtime marble motion uses the engine
/// [`IPhysicsScene`](../../engine/physics/IPhysicsScene.hpp) (Jolt implementation; see [`ADR-0058`](../../docs/decisions/ADR-0058-physics-middleware-integration.md)).
///
/// **Terrain direction:** heightfield-first — uneven ground should become a sampled height grid + static heightfield
/// collider in the physics backend; keep full triangle meshes for props, overhangs, and hero pieces only.
///
/// Prop meshes use unit `[-0.5,0.5]³` space (see `GardenGame::initGraphics`). `staticColliders` are the world AABB of
/// that box after the same per-axis scale and `Ry·Rx·Rz` as the renderer.
void buildGardenLayout(std::uint32_t seed, GardenLayout& out) noexcept;

/// Bilinear height on `terrain` at world XZ (clamped to the grid).
[[nodiscard]] float gardenTerrainHeightAt(GardenTerrain const& terrain, float worldX, float worldZ) noexcept;

/// Approximate terrain slope magnitude `sqrt((∂h/∂x)² + (∂h/∂z)²)` at world XZ (finite differences).
[[nodiscard]] float gardenTerrainSlopeMagnitude(GardenTerrain const& terrain, float worldX, float worldZ) noexcept;

/// Re-run **log-only** gravity settle vs all non-log colliders (e.g. after edits). `buildGardenLayout` runs this once
/// after logs are spawned and before small static debris is added.
void settleGardenDebrisInPlace(GardenLayout& layout) noexcept;

/// Default starting poses: two marbles opposite in the arena, resting on the heightfield.
void placeMarblesInArena(std::array<physics::RigidBodyKinematics, 2>& marbles, GardenLayout const& layout) noexcept;

/// Rough grounded test vs heightfield (props may read as airborne; good enough for jump gating).
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
