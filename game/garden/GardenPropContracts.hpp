#pragma once

#include "garden/GardenSimulation.hpp"

#include <array>

namespace marble::garden {

/// Visual mesh paired with spherical rock proxies (~inscribed sphere in half-extents box). Octahedron is intentionally
/// not used in proc layouts so silhouette matches the sphere-backed physics/draw path.
inline constexpr GardenPropMesh kGardenProcSphereRoughRockMesh = GardenPropMesh::Icosahedron;

/// High-level terrain sit policy ([`buildGardenLayout`]) aligned with Milestone B of the tangibility plan.
enum class GardenPropPlacementKind : std::uint8_t {
    AxisAlignedBricksOnTerrain,
    MeshBackedHeroOnTerrain,
    RoughRockCubeOrSphereOnTerrain,
    FallenCapsuleOnTerrain,
    VerticalTrunkCapsuleOnTerrain,
    FoliageSphereOnTerrain,
    GroundedLeafSphereOnTerrain,
};

/// How render picks scale from layout/proxy (see `GardenLayout` doc).
enum class GardenPropRenderScalePolicy : std::uint8_t {
    UnitBoxWorldAabb,
    OrientedUnitBoxProxyHalfExtents,
    SphereProxyRadius,
    CapsuleProxyCylinder,
};

/// One row per [`GardenColliderKind`] produced by [`buildGardenLayout`] (documentation + static checks).
struct GardenStaticPropContract {
    GardenColliderKind collider;
    GardenPropPlacementKind placement;
    GardenPropRenderScalePolicy render;
};

/// Fixed ordering: every kind that appears in procedural layout has exactly one row.
inline constexpr std::array<GardenStaticPropContract, 9> kGardenStaticPropContracts{{
    {GardenColliderKind::Wall,
     GardenPropPlacementKind::AxisAlignedBricksOnTerrain,
     GardenPropRenderScalePolicy::UnitBoxWorldAabb},
    {GardenColliderKind::HeroPlayground,
     GardenPropPlacementKind::MeshBackedHeroOnTerrain,
     GardenPropRenderScalePolicy::OrientedUnitBoxProxyHalfExtents},
    {GardenColliderKind::Boulder,
     GardenPropPlacementKind::RoughRockCubeOrSphereOnTerrain,
     GardenPropRenderScalePolicy::SphereProxyRadius},
    {GardenColliderKind::Rock,
     GardenPropPlacementKind::RoughRockCubeOrSphereOnTerrain,
     GardenPropRenderScalePolicy::SphereProxyRadius},
    {GardenColliderKind::Log,
     GardenPropPlacementKind::FallenCapsuleOnTerrain,
     GardenPropRenderScalePolicy::CapsuleProxyCylinder},
    {GardenColliderKind::Twig,
     GardenPropPlacementKind::FallenCapsuleOnTerrain,
     GardenPropRenderScalePolicy::CapsuleProxyCylinder},
    {GardenColliderKind::TreeTrunk,
     GardenPropPlacementKind::VerticalTrunkCapsuleOnTerrain,
     GardenPropRenderScalePolicy::CapsuleProxyCylinder},
    {GardenColliderKind::TreeFoliage,
     GardenPropPlacementKind::FoliageSphereOnTerrain,
     GardenPropRenderScalePolicy::SphereProxyRadius},
    {GardenColliderKind::Leaf,
     GardenPropPlacementKind::GroundedLeafSphereOnTerrain,
     GardenPropRenderScalePolicy::SphereProxyRadius},
}};

[[nodiscard]] inline bool gardenStaticLayoutPhysicsMatchesContract(
    GardenColliderKind kind,
    GardenStaticPhysicsProxy const& px,
    GardenPropMesh mesh
) noexcept {
    using CK = GardenColliderKind;
    using PK = GardenStaticPhysicsProxyKind;
    switch (kind) {
    case CK::Wall:
        return px.kind == PK::AabbBox;
    case CK::HeroPlayground:
        return px.kind == PK::TriangleMeshFromMeshBytes || px.kind == PK::AabbBox;
    case CK::Boulder:
    case CK::Rock:
        if (mesh == GardenPropMesh::Cube) {
            return px.kind == PK::OrientedBox;
        }
        return px.kind == PK::Sphere && mesh == kGardenProcSphereRoughRockMesh;
    case CK::Log:
    case CK::Twig:
    case CK::TreeTrunk:
        return px.kind == PK::Capsule;
    case CK::TreeFoliage:
    case CK::Leaf:
        return px.kind == PK::Sphere;
    default:
        return false;
    }
}

} // namespace marble::garden
