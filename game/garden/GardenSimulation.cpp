#include "garden/GardenSimulation.hpp"

#include "garden/GardenPropContracts.hpp"
#include "core/MeshAssetV1.hpp"
#include "core/MeshAssetV1GardenBake.hpp"
#include "core/MeshAssetV1PhysicsExtract.hpp"
#include "core/MeshAssetV1Write.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "math/Mat4.hpp"
#include "math/Vec3.hpp"
#include "physics/IPhysicsScene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace marble::garden {

namespace {

using math::Aabb;
using math::Vec3;

// --- Terrain contact for static layout (heightfield `gardenTerrainHeightAt` only) ---
// - `gardenOrientedUnitBoxSitOnTerrain`: **world-axis AABB footprint** max under XZ; cheap; good for *roughly* axis-aligned
//   props (ramp, bench, many trees) but **over-lifts** tilted boxes on slopes.
// - `gardenOrientedUnitBoxSitOnTerrainSupportSamples`: 20 OBB support probes (corners + edge mids); **lift** then
//   **sink** so tilted units sit without footprint hover. Use for boulder/scatter cube mesh, log/twig OBB, bury clamp.
// - Capsule: `gardenSnapCapsuleCenterOnAnalyticTerrain` then `gardenCapsuleRelaxCenterYOnTerrain` (sampled surface vs
//   heightfield, not flat footprint for vertical compare).

/// World AABB of unit mesh space `[-0.5,0.5]³` scaled by `2*half` per axis, then `Ry·Rx·Rz` — matches `GardenGame` draw.
[[nodiscard]] Aabb worldAabbOfOrientedUnitBox(Vec3 center, Vec3 half, float yaw, float pitch, float roll) noexcept {
    marble::math::Mat4 const r =
        marble::math::Mat4::rotationY(yaw) * marble::math::Mat4::rotationX(pitch) * marble::math::Mat4::rotationZ(roll);
    Vec3 lo{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    Vec3 hi{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };
    for (int ix = 0; ix < 2; ++ix) {
        float const sx = ix ? half.x : -half.x;
        for (int iy = 0; iy < 2; ++iy) {
            float const sy = iy ? half.y : -half.y;
            for (int iz = 0; iz < 2; ++iz) {
                float const sz = iz ? half.z : -half.z;
                Vec3 const w =
                    center + marble::math::transformDirection(r, Vec3{sx, sy, sz});
                lo.x = std::min(lo.x, w.x);
                lo.y = std::min(lo.y, w.y);
                lo.z = std::min(lo.z, w.z);
                hi.x = std::max(hi.x, w.x);
                hi.y = std::max(hi.y, w.y);
                hi.z = std::max(hi.z, w.z);
            }
        }
    }
    return {lo, hi};
}

/// Local offsets for support tests: 8 box corners + 12 edge midpoints (catches edge-through-hill penetration).
[[nodiscard]] std::array<Vec3, 20> gardenUnitBoxSupportLocalOffsets(Vec3 const& half) noexcept {
    float const hx = half.x;
    float const hy = half.y;
    float const hz = half.z;
    std::array<Vec3, 20> p{};
    std::size_t i = 0;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                p[i++] = Vec3{static_cast<float>(sx) * hx, static_cast<float>(sy) * hy, static_cast<float>(sz) * hz};
            }
        }
    }
    for (int sy = -1; sy <= 1; sy += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            p[i++] = Vec3{0.f, static_cast<float>(sy) * hy, static_cast<float>(sz) * hz};
        }
    }
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            p[i++] = Vec3{static_cast<float>(sx) * hx, 0.f, static_cast<float>(sz) * hz};
        }
    }
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            p[i++] = Vec3{static_cast<float>(sx) * hx, static_cast<float>(sy) * hy, 0.f};
        }
    }
    return p;
}

/// [0,1) for understory leaf radial scale only — not from the shared layout `rng`.
/// When boulder/scatter code tweaks change overlap retries, the global RNG stream shifts and every later draw
/// (including `0.08 + u01(rng) * 0.14` litter size) jumped; this keeps leaf scale a function of `(seed, slot)` only.
[[nodiscard]] float gardenUnderstoryLeafSpanU01(std::uint32_t worldSeed, int litterIndex) noexcept {
    std::mt19937 g(worldSeed ^ (0x4C656600u + static_cast<std::uint32_t>(litterIndex + 1) * 2246822519u));
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    return u01(g);
}

/// Largest positive `(terrain + clearance) - worldY` over support probes; 0 if already clear.
[[nodiscard]] float gardenOrientedUnitBoxMaxTerrainClearanceDeficit(
    GardenTerrain const& terrain,
    Vec3 const& center,
    Vec3 const& half,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    float clearanceM
) noexcept {
    marble::math::Mat4 const rot =
        marble::math::Mat4::rotationY(yawRadians) * marble::math::Mat4::rotationX(pitchRadians) *
        marble::math::Mat4::rotationZ(rollRadians);
    std::array<Vec3, 20> const locals = gardenUnitBoxSupportLocalOffsets(half);
    float maxNeed = 0.f;
    for (Vec3 const& loc : locals) {
        Vec3 const off = marble::math::transformDirection(rot, loc);
        float const wx = center.x + off.x;
        float const wz = center.z + off.z;
        float const wy = center.y + off.y;
        float const ty = gardenTerrainHeightAt(terrain, wx, wz);
        maxNeed = std::max(maxNeed, ty + clearanceM - wy);
    }
    return std::max(0.f, maxNeed);
}

/// Minimum `worldY - (terrain + clearance)` over the same probes (negative ⇒ penetration at some probe).
[[nodiscard]] float gardenOrientedUnitBoxMinSurfaceHeightMargin(
    GardenTerrain const& terrain,
    Vec3 const& center,
    Vec3 const& half,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    float clearanceM
) noexcept {
    marble::math::Mat4 const rot =
        marble::math::Mat4::rotationY(yawRadians) * marble::math::Mat4::rotationX(pitchRadians) *
        marble::math::Mat4::rotationZ(rollRadians);
    std::array<Vec3, 20> const locals = gardenUnitBoxSupportLocalOffsets(half);
    float minMargin = std::numeric_limits<float>::max();
    for (Vec3 const& loc : locals) {
        Vec3 const off = marble::math::transformDirection(rot, loc);
        float const wx = center.x + off.x;
        float const wz = center.z + off.z;
        float const wy = center.y + off.y;
        float const ty = gardenTerrainHeightAt(terrain, wx, wz);
        minMargin = std::min(minMargin, wy - (ty + clearanceM));
    }
    if (minMargin == std::numeric_limits<float>::max()) {
        return 0.f;
    }
    return minMargin;
}

/// Sit by lifting only until corner/edge probes clear the heightfield, then optional sink to trim hover (no footprint lift).
void gardenOrientedUnitBoxSitOnTerrainSupportSamples(
    GardenTerrain const& terrain,
    Vec3& center,
    Vec3 const& half,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    float clearanceM
) noexcept {
    constexpr int kMaxIters = 24;
    for (int iter = 0; iter < kMaxIters; ++iter) {
        float const d = gardenOrientedUnitBoxMaxTerrainClearanceDeficit(
            terrain, center, half, yawRadians, pitchRadians, rollRadians, clearanceM);
        if (d < 1.0e-6f) {
            break;
        }
        center.y += d;
    }
    // Lower if hovering: lift-only pass can leave props high when they were placed from an over-conservative bound.
    for (int s = 0; s < 28; ++s) {
        float const air = gardenOrientedUnitBoxMinSurfaceHeightMargin(
            terrain, center, half, yawRadians, pitchRadians, rollRadians, clearanceM);
        if (air < 1.5e-4f) {
            break;
        }
        center.y -= std::min(air - 0.5e-4f, 0.06f);
        float const pen = gardenOrientedUnitBoxMaxTerrainClearanceDeficit(
            terrain, center, half, yawRadians, pitchRadians, rollRadians, clearanceM);
        if (pen > 0.f) {
            center.y += pen;
        }
    }
}

/// Raise `center.y` so the world AABB of the oriented unit box clears max terrain under its XZ footprint (+ clearance).
void gardenOrientedUnitBoxSitOnTerrain(
    GardenTerrain const& terrain,
    Vec3& center,
    Vec3 const& half,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    float clearanceM
) noexcept {
    constexpr int kMaxIters = 10;
    for (int iter = 0; iter < kMaxIters; ++iter) {
        Aabb const box = worldAabbOfOrientedUnitBox(center, half, yawRadians, pitchRadians, rollRadians);
        Aabb foot{};
        foot.min = {box.min.x, 0.f, box.min.z};
        foot.max = {box.max.x, 0.f, box.max.z};
        float const tyMax = gardenMaxTerrainHeightUnderFootprint(terrain, foot);
        float const needY = tyMax + clearanceM;
        if (box.min.y >= needY - 1.0e-4f) {
            break;
        }
        center.y += needY - box.min.y;
    }
}

/// Vertical center so a sphere of `radius` at (wx, centerY, wz) clears terrain under a tight contact ring (+ clearance).
void gardenSitSphereOnTerrain(
    GardenTerrain const& terrain,
    float wx,
    float wz,
    float& centerY,
    float radius,
    float clearanceM
) noexcept {
    float const ringR = std::clamp(radius * 0.82f, radius * 0.35f, radius * 0.95f);
    float tyMax = gardenTerrainHeightAt(terrain, wx, wz);
    for (int k = 0; k < 8; ++k) {
        float const a = static_cast<float>(k) * (6.2831853f / 8.f);
        float const cx = wx + std::cos(a) * ringR;
        float const cz = wz + std::sin(a) * ringR;
        tyMax = std::max(tyMax, gardenTerrainHeightAt(terrain, cx, cz));
    }
    centerY = tyMax + radius + clearanceM;
}

/// After lowering by `buryM`, keep sphere bottom clearing terrain under the same contact ring sampling as sit.
void gardenApplyBuryThenClampSphereOnTerrain(
    GardenTerrain const& terrain,
    float wx,
    float wz,
    float& centerY,
    float radius,
    float buryM,
    float clearanceM
) noexcept {
    centerY -= buryM;
    float const ringR = std::clamp(radius * 0.82f, radius * 0.35f, radius * 0.95f);
    for (int iter = 0; iter < 12; ++iter) {
        float tyMax = gardenTerrainHeightAt(terrain, wx, wz);
        for (int k = 0; k < 8; ++k) {
            float const a = static_cast<float>(k) * (6.2831853f / 8.f);
            float const cx = wx + std::cos(a) * ringR;
            float const cz = wz + std::sin(a) * ringR;
            tyMax = std::max(tyMax, gardenTerrainHeightAt(terrain, cx, cz));
        }
        float const bottomY = centerY - radius;
        float const needY = tyMax + clearanceM;
        if (bottomY >= needY - 1.0e-4f) {
            break;
        }
        centerY += needY - bottomY;
    }
}

/// After lowering by `buryM`, ensure oriented box support probes stay at or above terrain (+ clearance).
void gardenApplyBuryThenClampOrientedBoxOnTerrain(
    GardenTerrain const& terrain,
    Vec3& center,
    Vec3 const& half,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    float buryM,
    float clearanceM
) noexcept {
    center.y -= buryM;
    gardenOrientedUnitBoxSitOnTerrainSupportSamples(
        terrain, center, half, yawRadians, pitchRadians, rollRadians, clearanceM);
}

[[nodiscard]] bool aabbOverlap(Aabb const& a, Aabb const& b) noexcept {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y &&
        a.min.z <= b.max.z && a.max.z >= b.min.z;
}

constexpr float kRockSphereProxyInscribedScale = 0.98f;
constexpr float kFoliageSphereProxyScale = 0.92f;

/// Deterministic tuck depth at (wx,wz): varies prop placement visually without advancing proc-layout RNG streams.
[[nodiscard]] float gardenDecorEmbedTuckM(float wx, float wz, std::uint32_t worldSeed, std::uint32_t salt) noexcept {
    std::uint32_t const h =
        worldSeed ^ salt ^
        (static_cast<std::uint32_t>(std::lround(wx * 4133.f)) * 131541u +
            static_cast<std::uint32_t>(std::lround(wz * 6173.f)) * 97477u);
    float const frac = static_cast<float>(h % 9973u) * (1.f / 9972.f);
    return kGardenDecorGroundTuckMinM +
        frac * (kGardenDecorGroundTuckMaxM - kGardenDecorGroundTuckMinM);
}

[[nodiscard]] GardenStaticPhysicsProxy twigOrLogCapsuleProxy(
    float hx,
    float hy,
    float hz,
    float yaw,
    float pitch,
    float roll) noexcept {
    GardenStaticPhysicsProxy p{};
    p.kind = GardenStaticPhysicsProxyKind::Capsule;
    float const hLong = std::max(hx, std::max(hy, hz));
    float const crossSum = hx + hy + hz - hLong;
    float const crossAvg = 0.5f * crossSum;
    p.capsuleHalfHeight = hLong;
    p.capsuleRadius = std::max(crossAvg, 1.0e-4f);
    // Spine along the longest **body** box half-axis so the capsule matches log/twig layout half extents.
    if (hx >= hy && hx >= hz) {
        p.intrinsicCylinderAxis = 1u;
    } else if (hy >= hx && hy >= hz) {
        p.intrinsicCylinderAxis = 0u;
    } else {
        p.intrinsicCylinderAxis = 2u;
    }
    p.capsuleYaw = yaw;
    p.capsulePitch = pitch;
    p.capsuleRoll = roll;
    return p;
}

[[nodiscard]] GardenStaticPhysicsProxy trunkCapsuleProxy(
    float trunkHalfY,
    float trunkRadius,
    float yaw,
    float pitch,
    float roll) noexcept {
    GardenStaticPhysicsProxy p{};
    p.kind = GardenStaticPhysicsProxyKind::Capsule;
    p.intrinsicCylinderAxis = 0u;
    p.capsuleHalfHeight = trunkHalfY;
    p.capsuleRadius = std::max(trunkRadius, 1.0e-4f);
    p.capsuleYaw = yaw;
    p.capsulePitch = pitch;
    p.capsuleRoll = roll;
    return p;
}

/// Bodies that participate in load-time log settle (translation only); everything else is static for that phase.
[[nodiscard]] bool isLogBakeKind(GardenColliderKind k) noexcept {
    return k == GardenColliderKind::Log;
}

/// After log bake: re-seat using the **capsule proxy’s** oriented half extents (same box used for layout + physics),
/// then refresh the world AABB from that oriented box (avoids snapping the loose world AABB, which mismatches the mesh).
void snapLogColliderToTerrainFromCapsuleLayout(GardenLayout& layout, std::size_t idx) noexcept {
    if (idx >= layout.staticPhysicsProxies.size() || idx >= layout.staticColliders.size() || idx >= layout.propYaw.size() ||
        idx >= layout.propPitch.size() || idx >= layout.propRoll.size()) {
        return;
    }
    GardenStaticPhysicsProxy const& px = layout.staticPhysicsProxies[idx];
    if (px.kind != GardenStaticPhysicsProxyKind::Capsule) {
        return;
    }
    float const hh = std::max(px.capsuleHalfHeight, 1.0e-4f);
    float const r = std::max(px.capsuleRadius, 1.0e-4f);
    Vec3 half{};
    if (px.intrinsicCylinderAxis == 1u) {
        half = {hh, r, r};
    } else if (px.intrinsicCylinderAxis == 2u) {
        half = {r, r, hh};
    } else {
        half = {r, hh, r};
    }
    float const yaw = layout.propYaw[idx];
    float const pitch = layout.propPitch[idx];
    float const roll = layout.propRoll[idx];
    Vec3 c = (layout.staticColliders[idx].min + layout.staticColliders[idx].max) * 0.5f;
    gardenOrientedUnitBoxSitOnTerrainSupportSamples(
        layout.terrain, c, half, yaw, pitch, roll, kGardenDecorSitClearanceM);
    gardenSnapCapsuleCenterOnAnalyticTerrain(
        layout.terrain,
        c,
        hh,
        r,
        yaw,
        pitch,
        roll,
        px.intrinsicCylinderAxis,
        kGardenDecorSitClearanceM);
    gardenCapsuleRelaxCenterYOnTerrain(
        layout.terrain,
        c,
        hh,
        r,
        yaw,
        pitch,
        roll,
        px.intrinsicCylinderAxis,
        kGardenDecorSitClearanceM);
    layout.staticColliders[idx] = worldAabbOfOrientedUnitBox(c, half, yaw, pitch, roll);
}

/// Separate dynamic AABB (center, half) from solid static AABB; updates `center` and `vel` (inelastic + tangential damping).
bool separateDebrisFromStatic(
    Vec3& center,
    Vec3 const& half,
    Vec3& vel,
    Aabb const& solid,
    float tangentRetention,
    float restitution
) noexcept {
    Aabb dyn{center - half, center + half};
    if (!aabbOverlap(dyn, solid)) {
        return false;
    }
    float const ox = std::min(dyn.max.x, solid.max.x) - std::max(dyn.min.x, solid.min.x);
    float const oy = std::min(dyn.max.y, solid.max.y) - std::max(dyn.min.y, solid.min.y);
    float const oz = std::min(dyn.max.z, solid.max.z) - std::max(dyn.min.z, solid.min.z);
    Vec3 const solidCenter = (solid.min + solid.max) * 0.5f;
    Vec3 n{};
    float pen = 0.f;
    // When X/Y penetrations tie, prefer Y so resting stacks don’t flip between axis normals every frame.
    float constexpr kPenTie = 0.012f;
    if (ox <= oy && ox <= oz) {
        if ((oy - ox) < kPenTie && oy <= oz) {
            pen = oy;
            n = center.y < solidCenter.y ? Vec3{0.f, -1.f, 0.f} : Vec3{0.f, 1.f, 0.f};
        } else {
            pen = ox;
            n = center.x < solidCenter.x ? Vec3{-1.f, 0.f, 0.f} : Vec3{1.f, 0.f, 0.f};
        }
    } else if (oy <= oz) {
        pen = oy;
        n = center.y < solidCenter.y ? Vec3{0.f, -1.f, 0.f} : Vec3{0.f, 1.f, 0.f};
    } else {
        pen = oz;
        n = center.z < solidCenter.z ? Vec3{0.f, 0.f, -1.f} : Vec3{0.f, 0.f, 1.f};
    }
    center = center + n * pen;

    float const vn = math::dot(vel, n);
    if (vn < 0.f) {
        vel = vel - n * (vn * (1.f + restitution));
    }
    float const vn2 = math::dot(vel, n);
    Vec3 const tangential = vel - n * vn2;
    vel = n * vn2 + tangential * tangentRetention;
    return true;
}

/// Trunk `[−half]+RyRxRz` used for overlap AABBs and capsule proxy (same reconstruction as terrain snap passes).
[[nodiscard]] Vec3 logTrunkOrientedHalfFromProxy(GardenStaticPhysicsProxy const& px) noexcept {
    float const hh = std::max(px.capsuleHalfHeight, 1.0e-4f);
    float const r = std::max(px.capsuleRadius, 1.0e-4f);
    if (px.intrinsicCylinderAxis == 1u) {
        return {hh, r, r};
    }
    if (px.intrinsicCylinderAxis == 2u) {
        return {r, r, hh};
    }
    return {r, hh, r};
}

[[nodiscard]] bool separateMovingAxisAabbFromStaticSphere(
    Vec3& centerDyn,
    Vec3 const& halfDyn,
    Vec3& velDyn,
    Vec3 sphereCenterStatic,
    float sphereRadius,
    float tangentRetention,
    float restitution
) noexcept {
    if (sphereRadius <= 1.0e-6f) {
        return false;
    }
    float const hx = halfDyn.x;
    float const hy = halfDyn.y;
    float const hz = halfDyn.z;
    float const qx = std::clamp(sphereCenterStatic.x, centerDyn.x - hx, centerDyn.x + hx);
    float const qy = std::clamp(sphereCenterStatic.y, centerDyn.y - hy, centerDyn.y + hy);
    float const qz = std::clamp(sphereCenterStatic.z, centerDyn.z - hz, centerDyn.z + hz);
    Vec3 pq{qx, qy, qz};
    Vec3 d = sphereCenterStatic - pq;
    float dsq = marble::math::lengthSquared(d);
    float constexpr kEps = 1.0e-8f;

    Vec3 dir{};
    float pen{};
    if (dsq >= kEps) {
        float const dist = std::sqrt(dsq);
        pen = sphereRadius - dist;
        dir = d * (1.f / dist);
        if (pen <= 1.0e-6f) {
            return false;
        }
        centerDyn = centerDyn - dir * pen;
        float vn = marble::math::dot(velDyn, dir);
        if (vn < 0.f) {
            velDyn = velDyn - dir * (vn * (1.f + restitution));
        }
        vn = marble::math::dot(velDyn, dir);
        Vec3 const tangent = velDyn - dir * vn;
        velDyn = dir * vn + tangent * tangentRetention;
        return true;
    }

    // Sphere center clamped onto box interior surface: separate along shortest half-axis.
    float const dxLeft = sphereCenterStatic.x - (centerDyn.x - hx);
    float const dxRight = (centerDyn.x + hx) - sphereCenterStatic.x;
    float const dyBot = sphereCenterStatic.y - (centerDyn.y - hy);
    float const dyTop = (centerDyn.y + hy) - sphereCenterStatic.y;
    float const dzBa = sphereCenterStatic.z - (centerDyn.z - hz);
    float const dzFr = (centerDyn.z + hz) - sphereCenterStatic.z;
    struct Axis {
        int axis{};
        float push{};
        float flip{};
    };
    Axis best{0, dxRight, 1.f};
    if (dxLeft < best.push) {
        best = {0, dxLeft, -1.f};
    }
    if (dyTop < best.push) {
        best = {1, dyTop, 1.f};
    }
    if (dyBot < best.push) {
        best = {1, dyBot, -1.f};
    }
    if (dzFr < best.push) {
        best = {2, dzFr, 1.f};
    }
    if (dzBa < best.push) {
        best = {2, dzBa, -1.f};
    }
    float const amt = sphereRadius + best.push * 0.5f + 1.0e-4f;
    if (best.axis == 0) {
        centerDyn.x -= best.flip * amt;
        dir = {best.flip, 0.f, 0.f};
    } else if (best.axis == 1) {
        centerDyn.y -= best.flip * amt;
        dir = {0.f, best.flip, 0.f};
    } else {
        centerDyn.z -= best.flip * amt;
        dir = {0.f, 0.f, best.flip};
    }
    float vn = marble::math::dot(velDyn, dir);
    if (vn < 0.f) {
        velDyn = velDyn - dir * (vn * (1.f + restitution));
    }
    vn = marble::math::dot(velDyn, dir);
    Vec3 const tangent = velDyn - dir * vn;
    velDyn = dir * vn + tangent * tangentRetention;
    return true;
}

/// Inverse of [`worldAabbOfOrientedUnitBox`] for symmetrical trunk half extents: find center so the world AABB of the
/// oriented box matches `stored` within epsilon (used instead of translating only the loose world-axis AABB center).
[[nodiscard]] bool refineLogOrientedCenterFromCollider(
    GardenStaticPhysicsProxy const& px,
    float yawRadians,
    float pitchRadians,
    float rollRadians,
    math::Aabb const& stored,
    Vec3& inOutCenter
) noexcept {
    Vec3 const half = logTrunkOrientedHalfFromProxy(px);
    Vec3 cur = inOutCenter;
    float constexpr kStep = 0.065f;
    for (int it = 0; it < 28; ++it) {
        Aabb pred = worldAabbOfOrientedUnitBox(cur, half, yawRadians, pitchRadians, rollRadians);
        Vec3 delta{
            ((stored.min.x + stored.max.x) - (pred.min.x + pred.max.x)) * 0.5f,
            ((stored.min.y + stored.max.y) - (pred.min.y + pred.max.y)) * 0.5f,
            ((stored.min.z + stored.max.z) - (pred.min.z + pred.max.z)) * 0.5f};
        cur = cur + delta * kStep;
        float const el = std::fabs(delta.x) + std::fabs(delta.y) + std::fabs(delta.z);
        if (el < 2.5e-3f) {
            inOutCenter = cur;
            return true;
        }
    }
    inOutCenter = cur;
    return false;
}

void dampVelocityAlongNormal(Vec3& vel, Vec3 const& n, float tangentRetention) noexcept {
    float const vn2 = math::dot(vel, n);
    Vec3 const tangential = vel - n * vn2;
    vel = n * vn2 + tangential * tangentRetention;
}

void separateDebrisPairWeighted(
    Vec3& c0,
    Vec3 const& h0,
    Vec3& v0,
    float inv0,
    Vec3& c1,
    Vec3 const& h1,
    Vec3& v1,
    float inv1,
    float restitution,
    float tangentRetention
) noexcept {
    Aabb const a{c0 - h0, c0 + h0};
    Aabb const b{c1 - h1, c1 + h1};
    if (!aabbOverlap(a, b)) {
        return;
    }
    float const ox = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    float const oy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    float const oz = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
    Vec3 n{};
    float pen = 0.f;
    float constexpr kPenTie = 0.012f;
    if (ox <= oy && ox <= oz) {
        if ((oy - ox) < kPenTie && oy <= oz) {
            pen = oy;
            n = c0.y < c1.y ? Vec3{0.f, -1.f, 0.f} : Vec3{0.f, 1.f, 0.f};
        } else {
            pen = ox;
            n = c0.x < c1.x ? Vec3{-1.f, 0.f, 0.f} : Vec3{1.f, 0.f, 0.f};
        }
    } else if (oy <= oz) {
        pen = oy;
        n = c0.y < c1.y ? Vec3{0.f, -1.f, 0.f} : Vec3{0.f, 1.f, 0.f};
    } else {
        pen = oz;
        n = c0.z < c1.z ? Vec3{0.f, 0.f, -1.f} : Vec3{0.f, 0.f, 1.f};
    }
    float const invSum = inv0 + inv1;
    if (invSum <= 0.f) {
        return;
    }
    float const w0 = inv1 / invSum;
    float const w1 = inv0 / invSum;
    c0 = c0 - n * (pen * w0);
    c1 = c1 + n * (pen * w1);

    float const rel = math::dot(v1 - v0, n);
    float const spdSum =
        std::sqrt(math::lengthSquared(v0)) + std::sqrt(math::lengthSquared(v1));
    constexpr float kMinPairClosing = 0.035f;
    constexpr float kPairRestingSpd = 0.12f;
    if (rel < -kMinPairClosing && spdSum > kPairRestingSpd) {
        float const j = -(1.f + restitution) * rel / invSum;
        v0 = v0 + n * (j * inv0);
        v1 = v1 - n * (j * inv1);
    }
    dampVelocityAlongNormal(v0, n, tangentRetention);
    dampVelocityAlongNormal(v1, n, tangentRetention);
}

[[nodiscard]] std::uint32_t terrainHashU32(std::uint32_t x) noexcept {
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    return x;
}

[[nodiscard]] float terrainValueNoise(int ix, int iz, std::uint32_t seed) noexcept {
    std::uint32_t const h =
        terrainHashU32(static_cast<std::uint32_t>(ix) * 374761393u + static_cast<std::uint32_t>(iz) * 668265263u + seed);
    return static_cast<float>(h & 0xFFFFFFu) * (1.f / 16777216.f);
}

[[nodiscard]] float terrainSmoothNoise(float x, float z, std::uint32_t seed) noexcept {
    int const x0 = static_cast<int>(std::floor(x));
    int const z0 = static_cast<int>(std::floor(z));
    float const fx = x - static_cast<float>(x0);
    float const fz = z - static_cast<float>(z0);
    float const u = fx * fx * (3.f - 2.f * fx);
    float const v = fz * fz * (3.f - 2.f * fz);
    float const a = terrainValueNoise(x0, z0, seed);
    float const b = terrainValueNoise(x0 + 1, z0, seed);
    float const c = terrainValueNoise(x0, z0 + 1, seed);
    float const d = terrainValueNoise(x0 + 1, z0 + 1, seed);
    float const ab = a + (b - a) * u;
    float const cd = c + (d - c) * u;
    return ab + (cd - ab) * v;
}

[[nodiscard]] float terrainBaseHeight(float wx, float wz, std::uint32_t seed) noexcept {
    float const warpX =
        wx + terrainSmoothNoise(wx * 0.011f + 3.7f, wz * 0.011f - 1.9f, seed ^ 0xA11CEu) * 14.f;
    float const warpZ =
        wz + terrainSmoothNoise(wx * 0.012f - 2.1f, wz * 0.012f + 5.3f, seed ^ 0xBEEFu) * 14.f;
    float h = 0.f;
    float amp = 3.15f;
    float freq = 0.038f;
    for (int o = 0; o < 5; ++o) {
        h += terrainSmoothNoise(warpX * freq, warpZ * freq, seed + static_cast<std::uint32_t>(o) * 0x9E3779B1u) * amp;
        amp *= 0.47f;
        freq *= 2.07f;
    }
    float const d = std::sqrt(wx * wx + wz * wz);
    float const inner = std::clamp((d - kArenaRadius * 0.52f) / (kArenaRadius * 1.22f), 0.f, 1.f);
    float const soften = inner * inner * (3.f - 2.f * inner);
    h *= 0.32f + 0.68f * soften;
    float const edge = std::clamp((d - (kYardHalfExtent - 52.f)) / 48.f, 0.f, 1.f);
    h += edge * edge * 1.55f;
    return h;
}

void fillGardenTerrain(std::uint32_t seed, float R, GardenTerrain& out) noexcept {
    std::uint32_t const N = kGardenTerrainSampleCount;
    out.sampleCount = N;
    out.origin = {-R, 0.f, -R};
    out.cellSize = (2.f * R) / static_cast<float>(N - 1u);
    out.heights.assign(static_cast<std::size_t>(N) * static_cast<std::size_t>(N), 0.f);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    std::uniform_real_distribution<float> uAngle(0.f, 6.2831853f);
    std::uniform_real_distribution<float> uRadial(22.f, R - 12.f);

    struct Pit {
        float cx{};
        float cz{};
        float radius{};
        float depth{};
    };
    std::vector<Pit> pits{};
    pits.reserve(6);
    std::vector<Vec3> pitCentersCheck{};
    for (int pit = 0; pit < 5; ++pit) {
        int tries = 0;
        while (tries < 160) {
            ++tries;
            float const t = uAngle(rng);
            float const rad = uRadial(rng);
            Vec3 const pc{std::cos(t) * rad, 0.f, std::sin(t) * rad};
            float const pitR = 5.f + u01(rng) * 9.f;
            bool ok = true;
            for (Vec3 const& o : pitCentersCheck) {
                float const dx = pc.x - o.x;
                float const dz = pc.z - o.z;
                if (std::sqrt(dx * dx + dz * dz) < pitR + 14.f) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                continue;
            }
            pitCentersCheck.push_back(pc);
            pits.push_back(Pit{pc.x, pc.z, pitR, 0.55f + u01(rng) * 1.05f});
            break;
        }
    }

    for (std::uint32_t iz = 0; iz < N; ++iz) {
        for (std::uint32_t ix = 0; ix < N; ++ix) {
            float const wx = out.origin.x + static_cast<float>(ix) * out.cellSize;
            float const wz = out.origin.z + static_cast<float>(iz) * out.cellSize;
            float h = terrainBaseHeight(wx, wz, seed);
            for (Pit const& p : pits) {
                float const dx = wx - p.cx;
                float const dz = wz - p.cz;
                float const rr = dx * dx + dz * dz;
                float const sigma = p.radius * 0.62f;
                float const g = std::exp(-rr / (2.f * sigma * sigma + 1e-4f));
                h -= p.depth * g;
            }
            out.heights[static_cast<std::size_t>(iz) * static_cast<std::size_t>(N) + static_cast<std::size_t>(ix)] = h;
        }
    }

    int constexpr kHillCount = 9;
    constexpr Vec3 kTerrainPad{2.4f, 0.35f, 2.4f};
    std::vector<Aabb> plateauScratch{};
    for (int h = 0; h < kHillCount; ++h) {
        int attempts = 0;
        while (attempts < 140) {
            ++attempts;
            float const t = uAngle(rng);
            float const rad = uRadial(rng);
            float const spanX = 9.f + u01(rng) * 18.f;
            float const spanZ = 9.f + u01(rng) * 18.f;
            float const hillH = 1.1f + u01(rng) * 3.8f;
            Vec3 const c{std::cos(t) * rad, hillH * 0.5f, std::sin(t) * rad};
            Vec3 const half{spanX * 0.5f, hillH * 0.5f, spanZ * 0.5f};
            Aabb const candidate{c - half, c + half};
            bool overlaps = false;
            for (Aabb const& o : plateauScratch) {
                Aabb a = candidate;
                a.min = a.min - kTerrainPad;
                a.max = a.max + kTerrainPad;
                if (aabbOverlap(a, o)) {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) {
                continue;
            }
            plateauScratch.push_back(candidate);
            float const cx = c.x;
            float const cz = c.z;
            float const hx = half.x;
            float const hz = half.z;
            for (std::uint32_t iz = 0; iz < N; ++iz) {
                for (std::uint32_t ix = 0; ix < N; ++ix) {
                    float const wx = out.origin.x + static_cast<float>(ix) * out.cellSize;
                    float const wz = out.origin.z + static_cast<float>(iz) * out.cellSize;
                    float const ux = std::fabs(wx - cx) / std::max(hx, 0.01f);
                    float const uz = std::fabs(wz - cz) / std::max(hz, 0.01f);
                    float const edge = std::max(ux, uz);
                    if (edge >= 1.f) {
                        continue;
                    }
                    float const w = (1.f - edge);
                    float const smooth = w * w * (3.f - 2.f * w);
                    std::size_t const idx =
                        static_cast<std::size_t>(iz) * static_cast<std::size_t>(N) + static_cast<std::size_t>(ix);
                    out.heights[idx] += hillH * smooth;
                }
            }
            break;
        }
    }

    out.minHeight = std::numeric_limits<float>::max();
    out.maxHeight = std::numeric_limits<float>::lowest();
    for (float y : out.heights) {
        out.minHeight = std::min(out.minHeight, y);
        out.maxHeight = std::max(out.maxHeight, y);
    }
}

} // namespace

float gardenTerrainHeightAt(GardenTerrain const& terrain, float worldX, float worldZ) noexcept {
    if (terrain.sampleCount < 2u || terrain.heights.size() != static_cast<std::size_t>(terrain.sampleCount) * terrain.sampleCount) {
        return 0.f;
    }
    std::uint32_t const N = terrain.sampleCount;
    float const relX = (worldX - terrain.origin.x) / terrain.cellSize;
    float const relZ = (worldZ - terrain.origin.z) / terrain.cellSize;
    int const max0 = static_cast<int>(N) - 2;
    if (max0 < 0) {
        return 0.f;
    }
    int const x0 = std::clamp(static_cast<int>(std::floor(relX)), 0, max0);
    int const z0 = std::clamp(static_cast<int>(std::floor(relZ)), 0, max0);
    int const x1 = x0 + 1;
    int const z1 = z0 + 1;
    float const tx = relX - static_cast<float>(x0);
    float const tz = relZ - static_cast<float>(z0);
    auto const sample = [&](int ix, int iz) -> float {
        return terrain.heights[static_cast<std::size_t>(iz) * static_cast<std::size_t>(N) + static_cast<std::size_t>(ix)];
    };
    float const h00 = sample(x0, z0);
    float const h10 = sample(x1, z0);
    float const h01 = sample(x0, z1);
    float const h11 = sample(x1, z1);
    float const h0 = h00 + (h10 - h00) * tx;
    float const h1 = h01 + (h11 - h01) * tx;
    return h0 + (h1 - h0) * tz;
}

float gardenTerrainSlopeMagnitude(GardenTerrain const& terrain, float worldX, float worldZ) noexcept {
    float const e = std::max(terrain.cellSize * 1.5f, 0.08f);
    float const hx =
        (gardenTerrainHeightAt(terrain, worldX + e, worldZ) - gardenTerrainHeightAt(terrain, worldX - e, worldZ)) /
        (2.f * e);
    float const hz =
        (gardenTerrainHeightAt(terrain, worldX, worldZ + e) - gardenTerrainHeightAt(terrain, worldX, worldZ - e)) /
        (2.f * e);
    return std::sqrt(hx * hx + hz * hz);
}

math::Vec3 gardenTerrainUpNormal(GardenTerrain const& terrain, float worldX, float worldZ) noexcept {
    float const e = std::max(terrain.cellSize * 1.5f, 0.08f);
    float const hx =
        (gardenTerrainHeightAt(terrain, worldX + e, worldZ) - gardenTerrainHeightAt(terrain, worldX - e, worldZ)) /
        (2.f * e);
    float const hz =
        (gardenTerrainHeightAt(terrain, worldX, worldZ + e) - gardenTerrainHeightAt(terrain, worldX, worldZ - e)) /
        (2.f * e);
    return marble::math::normalize(math::Vec3{-hx, 1.f, -hz});
}

float gardenMaxTerrainHeightUnderFootprint(GardenTerrain const& terrain, math::Aabb const& foot) noexcept {
    float const x0 = foot.min.x;
    float const x1 = foot.max.x;
    float const z0 = foot.min.z;
    float const z1 = foot.max.z;
    float const xc = (x0 + x1) * 0.5f;
    float const zc = (z0 + z1) * 0.5f;
    float m = gardenTerrainHeightAt(terrain, xc, zc);
    m = std::max(m, gardenTerrainHeightAt(terrain, x0, z0));
    m = std::max(m, gardenTerrainHeightAt(terrain, x1, z0));
    m = std::max(m, gardenTerrainHeightAt(terrain, x0, z1));
    m = std::max(m, gardenTerrainHeightAt(terrain, x1, z1));
    m = std::max(m, gardenTerrainHeightAt(terrain, xc, z0));
    m = std::max(m, gardenTerrainHeightAt(terrain, xc, z1));
    m = std::max(m, gardenTerrainHeightAt(terrain, x0, zc));
    m = std::max(m, gardenTerrainHeightAt(terrain, x1, zc));
    return m;
}

float gardenCapsuleLowestWorldY(
    math::Vec3 center,
    math::Vec3 worldAxisUnnormalized,
    float halfHeight,
    float radius
) noexcept {
    using marble::math::cross;
    float const hh = std::max(halfHeight, 1.0e-4f);
    float const r = std::max(radius, 1.0e-4f);
    float len2 = worldAxisUnnormalized.x * worldAxisUnnormalized.x + worldAxisUnnormalized.y * worldAxisUnnormalized.y +
        worldAxisUnnormalized.z * worldAxisUnnormalized.z;
    if (len2 < 1.0e-16f) {
        return center.y - r;
    }
    float const invLen = 1.f / std::sqrt(len2);
    math::Vec3 const a{
        worldAxisUnnormalized.x * invLen,
        worldAxisUnnormalized.y * invLen,
        worldAxisUnnormalized.z * invLen,
    };
    math::Vec3 const p0{center.x - a.x * hh, center.y - a.y * hh, center.z - a.z * hh};
    math::Vec3 const p1{center.x + a.x * hh, center.y + a.y * hh, center.z + a.z * hh};
    float m = std::min(p0.y - r, p1.y - r);
    math::Vec3 ref{0.f, 1.f, 0.f};
    if (std::fabs(a.y) > 0.92f) {
        ref = {1.f, 0.f, 0.f};
    }
    math::Vec3 u = cross(a, ref);
    float ul2 = u.x * u.x + u.y * u.y + u.z * u.z;
    if (ul2 < 1.0e-16f) {
        return m;
    }
    float const invU = 1.f / std::sqrt(ul2);
    u.x *= invU;
    u.y *= invU;
    u.z *= invU;
    math::Vec3 const w = cross(a, u);
    for (int ti = 0; ti <= 16; ++ti) {
        float const t = ti / 16.f;
        math::Vec3 const base{
            p0.x + (p1.x - p0.x) * t,
            p0.y + (p1.y - p0.y) * t,
            p0.z + (p1.z - p0.z) * t,
        };
        for (int vi = 0; vi < 16; ++vi) {
            float const ang = static_cast<float>(vi) * (6.283185307179586f / 16.f);
            float const ca = std::cos(ang);
            float const sa = std::sin(ang);
            math::Vec3 const v{u.x * ca + w.x * sa, u.y * ca + w.y * sa, u.z * ca + w.z * sa};
            m = std::min(m, base.y + v.y * r);
        }
    }
    return m;
}

/// Max `(terrain + clearance) - surfaceY` over the same capsule surface samples as [`gardenCapsuleLowestWorldY`].
[[nodiscard]] float gardenCapsuleMaxTerrainClearanceDeficit(
    GardenTerrain const& terrain,
    math::Vec3 center,
    math::Vec3 worldAxisUnnormalized,
    float halfHeight,
    float radius,
    float clearanceM
) noexcept {
    using marble::math::cross;
    float const hh = std::max(halfHeight, 1.0e-4f);
    float const r = std::max(radius, 1.0e-4f);
    float len2 = worldAxisUnnormalized.x * worldAxisUnnormalized.x + worldAxisUnnormalized.y * worldAxisUnnormalized.y +
        worldAxisUnnormalized.z * worldAxisUnnormalized.z;
    if (len2 < 1.0e-16f) {
        float const ty = gardenTerrainHeightAt(terrain, center.x, center.z);
        return std::max(0.f, ty + clearanceM - (center.y - r));
    }
    float const invLen = 1.f / std::sqrt(len2);
    math::Vec3 const a{
        worldAxisUnnormalized.x * invLen,
        worldAxisUnnormalized.y * invLen,
        worldAxisUnnormalized.z * invLen,
    };
    math::Vec3 const p0{center.x - a.x * hh, center.y - a.y * hh, center.z - a.z * hh};
    math::Vec3 const p1{center.x + a.x * hh, center.y + a.y * hh, center.z + a.z * hh};
    float maxNeed = 0.f;
    auto consider = [&](float wx, float wy, float wz) {
        float const ty = gardenTerrainHeightAt(terrain, wx, wz);
        maxNeed = std::max(maxNeed, ty + clearanceM - wy);
    };
    math::Vec3 ref{0.f, 1.f, 0.f};
    if (std::fabs(a.y) > 0.92f) {
        ref = {1.f, 0.f, 0.f};
    }
    math::Vec3 u = cross(a, ref);
    float ul2 = u.x * u.x + u.y * u.y + u.z * u.z;
    if (ul2 < 1.0e-16f) {
        consider(p0.x, p0.y - r, p0.z);
        consider(p1.x, p1.y - r, p1.z);
        return std::max(0.f, maxNeed);
    }
    float const invU = 1.f / std::sqrt(ul2);
    u.x *= invU;
    u.y *= invU;
    u.z *= invU;
    math::Vec3 const w = cross(a, u);
    for (int ti = 0; ti <= 16; ++ti) {
        float const t = ti / 16.f;
        math::Vec3 const base{
            p0.x + (p1.x - p0.x) * t,
            p0.y + (p1.y - p0.y) * t,
            p0.z + (p1.z - p0.z) * t,
        };
        for (int vi = 0; vi < 16; ++vi) {
            float const ang = static_cast<float>(vi) * (6.283185307179586f / 16.f);
            float const ca = std::cos(ang);
            float const sa = std::sin(ang);
            math::Vec3 const v{u.x * ca + w.x * sa, u.y * ca + w.y * sa, u.z * ca + w.z * sa};
            consider(base.x + v.x * r, base.y + v.y * r, base.z + v.z * r);
        }
    }
    return std::max(0.f, maxNeed);
}

/// Minimum `surfaceY - (terrain + clearance)` over capsule samples (positive ⇒ hovering at tightest probe).
[[nodiscard]] float gardenCapsuleMinSurfaceHeightMargin(
    GardenTerrain const& terrain,
    math::Vec3 center,
    math::Vec3 worldAxisUnnormalized,
    float halfHeight,
    float radius,
    float clearanceM
) noexcept {
    using marble::math::cross;
    float const hh = std::max(halfHeight, 1.0e-4f);
    float const r = std::max(radius, 1.0e-4f);
    float len2 = worldAxisUnnormalized.x * worldAxisUnnormalized.x + worldAxisUnnormalized.y * worldAxisUnnormalized.y +
        worldAxisUnnormalized.z * worldAxisUnnormalized.z;
    if (len2 < 1.0e-16f) {
        float const ty = gardenTerrainHeightAt(terrain, center.x, center.z);
        return (center.y - r) - (ty + clearanceM);
    }
    float const invLen = 1.f / std::sqrt(len2);
    math::Vec3 const a{
        worldAxisUnnormalized.x * invLen,
        worldAxisUnnormalized.y * invLen,
        worldAxisUnnormalized.z * invLen,
    };
    math::Vec3 const p0{center.x - a.x * hh, center.y - a.y * hh, center.z - a.z * hh};
    math::Vec3 const p1{center.x + a.x * hh, center.y + a.y * hh, center.z + a.z * hh};
    float minMargin = std::numeric_limits<float>::max();
    auto consider = [&](float wx, float wy, float wz) {
        float const ty = gardenTerrainHeightAt(terrain, wx, wz);
        minMargin = std::min(minMargin, wy - (ty + clearanceM));
    };
    math::Vec3 ref{0.f, 1.f, 0.f};
    if (std::fabs(a.y) > 0.92f) {
        ref = {1.f, 0.f, 0.f};
    }
    math::Vec3 u = cross(a, ref);
    float ul2 = u.x * u.x + u.y * u.y + u.z * u.z;
    if (ul2 < 1.0e-16f) {
        consider(p0.x, p0.y - r, p0.z);
        consider(p1.x, p1.y - r, p1.z);
        return minMargin == std::numeric_limits<float>::max() ? 0.f : minMargin;
    }
    float const invU = 1.f / std::sqrt(ul2);
    u.x *= invU;
    u.y *= invU;
    u.z *= invU;
    math::Vec3 const w = cross(a, u);
    for (int ti = 0; ti <= 16; ++ti) {
        float const t = ti / 16.f;
        math::Vec3 const base{
            p0.x + (p1.x - p0.x) * t,
            p0.y + (p1.y - p0.y) * t,
            p0.z + (p1.z - p0.z) * t,
        };
        for (int vi = 0; vi < 16; ++vi) {
            float const ang = static_cast<float>(vi) * (6.283185307179586f / 16.f);
            float const ca = std::cos(ang);
            float const sa = std::sin(ang);
            math::Vec3 const v{u.x * ca + w.x * sa, u.y * ca + w.y * sa, u.z * ca + w.z * sa};
            consider(base.x + v.x * r, base.y + v.y * r, base.z + v.z * r);
        }
    }
    return minMargin == std::numeric_limits<float>::max() ? 0.f : minMargin;
}

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
) noexcept {
    math::Vec3 meshAxis{0.f, 1.f, 0.f};
    if (intrinsicCylinderAxis == 1u) {
        meshAxis = {1.f, 0.f, 0.f};
    } else if (intrinsicCylinderAxis == 2u) {
        meshAxis = {0.f, 0.f, 1.f};
    }
    math::Mat4 const rot = math::Mat4::rotationY(yawRadians) * math::Mat4::rotationX(pitchRadians) *
        math::Mat4::rotationZ(rollRadians);
    math::Vec3 wAxis = marble::math::transformDirection(rot, meshAxis);
    float const wh = std::max(halfHeight, 1.0e-4f);
    float const wr = std::max(radius, 1.0e-4f);
    constexpr int kMaxIters = 28;
    for (int iter = 0; iter < kMaxIters; ++iter) {
        float const d = gardenCapsuleMaxTerrainClearanceDeficit(terrain, center, wAxis, wh, wr, clearanceM);
        if (d < 1.0e-6f) {
            break;
        }
        center.y += d;
    }
}

/// Lift out of penetration then sink toward terrain using the same capsule samples (no footprint over-lift).
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
) noexcept {
    math::Vec3 meshAxis{0.f, 1.f, 0.f};
    if (intrinsicCylinderAxis == 1u) {
        meshAxis = {1.f, 0.f, 0.f};
    } else if (intrinsicCylinderAxis == 2u) {
        meshAxis = {0.f, 0.f, 1.f};
    }
    math::Mat4 const rot = math::Mat4::rotationY(yawRadians) * math::Mat4::rotationX(pitchRadians) *
        math::Mat4::rotationZ(rollRadians);
    math::Vec3 const wAxis = marble::math::transformDirection(rot, meshAxis);
    float const wh = std::max(halfHeight, 1.0e-4f);
    float const wr = std::max(radius, 1.0e-4f);
    for (int i = 0; i < 56; ++i) {
        float const pen = gardenCapsuleMaxTerrainClearanceDeficit(terrain, center, wAxis, wh, wr, clearanceM);
        if (pen > 1.0e-6f) {
            center.y += pen;
            continue;
        }
        float const air = gardenCapsuleMinSurfaceHeightMargin(terrain, center, wAxis, wh, wr, clearanceM);
        if (air > 1.5e-4f) {
            center.y -= std::min(air - 0.5e-4f, 0.09f);
            continue;
        }
        break;
    }
}

namespace {

/// Columns are images of **+X**, **+Y**, **+Z** under `Ry*Rx*Rz` (same convention as [`worldAabbOfOrientedUnitBox`]).
void gardenYprFromRyRxRzColumns(math::Vec3 c0, math::Vec3 c1, math::Vec3 c2, float& yaw, float& pitch, float& roll) noexcept {
    using marble::math::cross;
    using marble::math::dot;
    using marble::math::lengthSquared;
    using marble::math::normalize;
    c0 = normalize(c0);
    c1 = normalize(c1);
    c1 = normalize(c1 - c0 * dot(c0, c1));
    c2 = normalize(cross(c0, c1));
    float const m4 = c0.y;
    float const m5 = c1.y;
    float const m6 = c2.y;
    float const m2 = c2.x;
    float const m10 = c2.z;
    float const cpMag = std::sqrt(std::max(0.f, m2 * m2 + m10 * m10));
    if (cpMag < 1.0e-5f) {
        yaw = std::atan2(-c2.x, c2.z);
        pitch = (m6 > 0.f) ? 1.5707963f : -1.5707963f;
        roll = std::atan2(-m4, std::max(1.0e-6f, std::fabs(m5)));
        return;
    }
    roll = std::atan2(-m4, m5);
    pitch = std::atan2(m6, cpMag);
    yaw = std::atan2(-m2, m10);
}

/// Resting orientation: local **+Y** tends toward terrain normal `nWorld`; `terrainAlignWeight` blends with world
/// +Y (0 = upright regardless of slope, 1 = full ground normal — often reads as awkwardly “standing” on hills).
void gardenRockRestYprFromNormal(
    math::Vec3 nWorld,
    float rollTwist,
    float terrainAlignWeight,
    float& yaw,
    float& pitch,
    float& roll) noexcept {
    using marble::math::Vec3;
    using marble::math::cross;
    using marble::math::dot;
    using marble::math::lengthSquared;
    using marble::math::normalize;
    nWorld = normalize(nWorld);
    if (lengthSquared(nWorld) < 1.0e-10f) {
        nWorld = Vec3{0.f, 1.f, 0.f};
    }
    Vec3 const yUp{0.f, 1.f, 0.f};
    float const t = std::clamp(terrainAlignWeight, 0.f, 1.f);
    Vec3 nBlend = yUp * (1.f - t) + nWorld * t;
    if (lengthSquared(nBlend) < 1.0e-10f) {
        nBlend = yUp;
    } else {
        nBlend = normalize(nBlend);
    }
    Vec3 const ref = std::fabs(dot(yUp, nBlend)) > 0.92f ? Vec3{1.f, 0.f, 0.f} : yUp;
    Vec3 c0w = normalize(cross(ref, nBlend));
    Vec3 c2w = normalize(cross(nBlend, c0w));
    float const ct = std::cos(rollTwist);
    float const st = std::sin(rollTwist);
    Vec3 const c0 = normalize(c0w * ct + c2w * st);
    Vec3 const c1 = nBlend;
    Vec3 const c2 = normalize(cross(c0, c1));
    gardenYprFromRyRxRzColumns(c0, c1, c2, yaw, pitch, roll);
}

/// Fallen capsule / branch: spine lies in the tangent plane; body **+Y** matches ground normal (thin axis “up”).
void gardenFallenCapsuleRestYpr(
    GardenTerrain const& terrain,
    float wx,
    float wz,
    float hx,
    float hy,
    float hz,
    float axisTwist,
    float& yaw,
    float& pitch,
    float& roll) noexcept {
    using marble::math::Vec3;
    using marble::math::cross;
    using marble::math::dot;
    using marble::math::lengthSquared;
    using marble::math::normalize;
    Vec3 const nRaw = gardenTerrainUpNormal(terrain, wx, wz);
    float constexpr kGroundNormalBlend = 0.41f;
    Vec3 const n = marble::math::normalize(
        Vec3{0.f, 1.f, 0.f} * (1.f - kGroundNormalBlend) + nRaw * kGroundNormalBlend);
    float const e = std::max(terrain.cellSize * 1.5f, 0.08f);
    float const dhdx =
        (gardenTerrainHeightAt(terrain, wx + e, wz) - gardenTerrainHeightAt(terrain, wx - e, wz)) / (2.f * e);
    float const dhdz =
        (gardenTerrainHeightAt(terrain, wx, wz + e) - gardenTerrainHeightAt(terrain, wx, wz - e)) / (2.f * e);
    Vec3 tPlan{-dhdz, 0.f, dhdx};
    tPlan = normalize(tPlan);
    if (lengthSquared(tPlan) < 1.0e-10f) {
        tPlan = Vec3{1.f, 0.f, 0.f};
    }
    Vec3 const c1 = normalize(n);
    Vec3 const side = normalize(cross(c1, tPlan));
    float const ct = std::cos(axisTwist);
    float const st = std::sin(axisTwist);
    Vec3 const t0 = normalize(tPlan * ct + side * st);
    GardenStaticPhysicsProxy const pxP = twigOrLogCapsuleProxy(hx, hy, hz, 0.f, 0.f, 0.f);
    std::uint8_t const ia = pxP.intrinsicCylinderAxis;
    if (ia == 0u) {
        gardenRockRestYprFromNormal(c1, axisTwist, 1.f, yaw, pitch, roll);
        return;
    }
    Vec3 c0{};
    Vec3 c2{};
    if (ia == 1u) {
        c0 = t0;
        c2 = normalize(cross(c0, c1));
    } else {
        c2 = t0;
        c0 = normalize(cross(c1, c2));
    }
    gardenYprFromRyRxRzColumns(c0, c1, c2, yaw, pitch, roll);
}

} // namespace

void buildGardenLayout(std::uint32_t seed, GardenLayout& out) noexcept {
    out.terrain = {};
    out.staticColliders.clear();
    out.kinds.clear();
    out.colliderTangentRetention.clear();
    out.propMesh.clear();
    out.propYaw.clear();
    out.propPitch.clear();
    out.propRoll.clear();
    out.propCollisionMeshBytes.clear();
    out.staticPhysicsProxies.clear();

    float const R = kYardHalfExtent;
    fillGardenTerrain(seed, R, out.terrain);

    auto push = [&](Aabb const& box, float tangentRetention, GardenColliderKind kind, GardenPropMesh mesh,
                    float yawRad, float pitchRad, float rollRad, GardenStaticPhysicsProxy const& proxy) {
        out.staticColliders.push_back(box);
        out.staticPhysicsProxies.push_back(proxy);
        out.colliderTangentRetention.push_back(tangentRetention);
        out.kinds.push_back(kind);
        out.propMesh.push_back(static_cast<std::uint8_t>(mesh));
        out.propYaw.push_back(yawRad);
        out.propPitch.push_back(pitchRad);
        out.propRoll.push_back(rollRad);
        out.propCollisionMeshBytes.emplace_back();
    };

    std::mt19937 rngHero(seed ^ 0x72A11E73u);
    std::uniform_real_distribution<float> uHero01(0.f, 1.f);
    std::uniform_real_distribution<float> uHeroAngle(0.f, 6.2831853f);

    std::mt19937 rngBoulder(seed ^ 0xB01DB01Du);
    std::uniform_real_distribution<float> uBoulder01(0.f, 1.f);
    std::uniform_real_distribution<float> uBoulderAngle(0.f, 6.2831853f);
    std::uniform_int_distribution<int> rockMeshPickBoulder(0, 9);

    std::mt19937 rngLog(seed ^ 0x109EE109u);
    std::uniform_real_distribution<float> uLog01(0.f, 1.f);
    std::uniform_real_distribution<float> uLogAngle(0.f, 6.2831853f);
    std::uniform_real_distribution<float> uLogRadDist(18.f, R - 14.f);

    std::mt19937 rngScatter(seed ^ 0x2C057E5Cu);
    std::uniform_real_distribution<float> uScatter01(0.f, 1.f);
    std::uniform_real_distribution<float> uScatterAngle(0.f, 6.2831853f);
    std::uniform_int_distribution<int> rockMeshPickScatter(0, 9);

    std::mt19937 rngTwig(seed ^ 0x3D068F6Du);
    std::uniform_real_distribution<float> uTwig01(0.f, 1.f);
    std::uniform_real_distribution<float> uTwigAngle(0.f, 6.2831853f);

    std::mt19937 rngTree(seed ^ 0x4E179E7Eu);
    std::uniform_real_distribution<float> uTree01(0.f, 1.f);
    std::uniform_real_distribution<float> uTreeAngle(0.f, 6.2831853f);
    std::uniform_real_distribution<float> uTreeRadial(14.f, R - 22.f);

    std::mt19937 rngUnder(seed ^ 0x5F18AF8Fu);
    std::uniform_real_distribution<float> uUnder01(0.f, 1.f);
    std::uniform_real_distribution<float> uUnderAngle(0.f, 6.2831853f);

    constexpr Vec3 kBoulderPad{1.85f, 1.2f, 1.85f};
    constexpr Vec3 kPropPad{0.45f, 0.12f, 0.45f};

    auto overlapsAny = [&](Aabb const& candidate, Vec3 pad) -> bool {
        Aabb inflated = candidate;
        inflated.min = inflated.min - pad;
        inflated.max = inflated.max + pad;
        for (std::size_t ei = 0; ei < out.staticColliders.size(); ++ei) {
            if (aabbOverlap(inflated, out.staticColliders[ei])) {
                return true;
            }
        }
        return false;
    };

    // Perimeter wall (human-scale brick courses).
    int const kWallSegments = 56;
    int const kWallCourses = 2;
    float const wallInnerR = R - 2.2f;
    float const courseH = 0.38f;
    float const brickW = 1.05f;
    float const brickD = 0.58f;
    for (int row = 0; row < kWallCourses; ++row) {
        float const phase = (row % 2 != 0) ? (3.14159265f / static_cast<float>(kWallSegments)) : 0.f;
        for (int i = 0; i < kWallSegments; ++i) {
            float const a = phase + static_cast<float>(i) * (6.2831853f / static_cast<float>(kWallSegments));
            float const cx = std::cos(a) * (wallInnerR + brickD * 0.5f);
            float const cz = std::sin(a) * (wallInnerR + brickD * 0.5f);
            float const groundY = gardenTerrainHeightForStaticPlacement(out.terrain, cx, cz);
            std::uint32_t const tuckHash =
                static_cast<std::uint32_t>(seed) ^
                (static_cast<std::uint32_t>(row) * 131541u +
                 static_cast<std::uint32_t>(static_cast<unsigned>(i)) * 97477u);
            float const tuckFrac =
                static_cast<float>(tuckHash & 65535u) * (1.f / 65535.f); // deterministic [0,1]
            float const wallTuck = kGardenDecorGroundTuckMinM +
                tuckFrac * (kGardenDecorGroundTuckMaxM - kGardenDecorGroundTuckMinM);
            float const yBase = groundY + 0.04f - wallTuck;
            float const yCenter = yBase + courseH * 0.5f + static_cast<float>(row) * (courseH - 0.03f);
            Vec3 const center{cx, yCenter, cz};
            Vec3 half{brickW * 0.5f, courseH * 0.5f, brickD * 0.5f};
            push(
                {center - half, center + half},
                0.84f,
                GardenColliderKind::Wall,
                GardenPropMesh::Cube,
                0.f,
                0.f,
                0.f,
                GardenStaticPhysicsProxy{});
        }
    }

    std::vector<std::uint8_t> const kGardenRampTemplate = marble::core::meshAssetV1BuildGardenTiltedRampTemplateBytes();
    std::vector<std::uint8_t> const kGardenBenchTemplate = marble::core::meshAssetV1BuildGardenBenchSlatsTemplateBytes();

    {
        bool placedRamp = false;
        for (int tries = 0; tries < 160 && !placedRamp; ++tries) {
            float const t = uHeroAngle(rngHero);
            float const rad = 7.5f + uHero01(rngHero) * 14.f;
            Vec3 c{std::cos(t) * rad, 0.f, std::sin(t) * rad};
            Vec3 half{1.05f, 0.08f, 0.52f};
            float const yawR = uHeroAngle(rngHero);
            float const pitchR = 0.09f + uHero01(rngHero) * 0.2f;
            float const rollR = (uHero01(rngHero) - 0.5f) * 0.28f;
            float const ty = gardenTerrainHeightForStaticPlacement(out.terrain, c.x, c.z);
            c.y = ty + half.y + 0.05f;
            gardenOrientedUnitBoxSitOnTerrain(
                out.terrain, c, half, yawR, pitchR, rollR, kGardenDecorSitClearanceM);
            float const heroTuckRamp = gardenDecorEmbedTuckM(c.x, c.z, seed, 0xC4B20D33u);
            gardenApplyBuryThenClampOrientedBoxOnTerrain(
                out.terrain,
                c,
                half,
                yawR,
                pitchR,
                rollR,
                heroTuckRamp,
                kGardenDecorSitClearanceM);
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawR, pitchR, rollR);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            GardenStaticPhysicsProxy pxRamp{};
            pxRamp.kind = GardenStaticPhysicsProxyKind::TriangleMeshFromMeshBytes;
            pxRamp.orientedHalfExtents = half;
            pxRamp.orientedYaw = yawR;
            pxRamp.orientedPitch = pitchR;
            pxRamp.orientedRoll = rollR;
            push(
                candidate,
                0.9f,
                GardenColliderKind::HeroPlayground,
                GardenPropMesh::MeshTiltedRamp,
                yawR,
                pitchR,
                rollR,
                pxRamp);
            std::vector<std::uint8_t> baked;
            if (marble::core::meshAssetV1BakeGardenPropInstanceToWorldBytes(
                    std::span<std::uint8_t const>(kGardenRampTemplate.data(), kGardenRampTemplate.size()),
                    math::Vec3{c.x, c.y, c.z},
                    math::Vec3{half.x, half.y, half.z},
                    yawR,
                    pitchR,
                    rollR,
                    baked)) {
                out.propCollisionMeshBytes.back() = std::move(baked);
            } else {
                out.staticPhysicsProxies.back().kind = GardenStaticPhysicsProxyKind::AabbBox;
                out.propCollisionMeshBytes.back().clear();
            }
            placedRamp = true;
        }
    }
    {
        bool placedBench = false;
        for (int tries = 0; tries < 200 && !placedBench; ++tries) {
            float const t = uHeroAngle(rngHero);
            float const rad = 9.f + uHero01(rngHero) * 16.f;
            Vec3 c{std::cos(t) * rad, 0.f, std::sin(t) * rad};
            Vec3 half{0.94f, 0.46f, 0.48f};
            float const yawR = uHeroAngle(rngHero);
            float const pitchR = (uHero01(rngHero) - 0.5f) * 0.12f;
            float const rollR = (uHero01(rngHero) - 0.5f) * 0.18f;
            float const ty = gardenTerrainHeightForStaticPlacement(out.terrain, c.x, c.z);
            c.y = ty + half.y + 0.04f;
            gardenOrientedUnitBoxSitOnTerrain(
                out.terrain, c, half, yawR, pitchR, rollR, kGardenDecorSitClearanceM);
            float const heroTuckBench = gardenDecorEmbedTuckM(c.x, c.z, seed, 0xD5C39E44u);
            gardenApplyBuryThenClampOrientedBoxOnTerrain(
                out.terrain,
                c,
                half,
                yawR,
                pitchR,
                rollR,
                heroTuckBench,
                kGardenDecorSitClearanceM);
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawR, pitchR, rollR);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            GardenStaticPhysicsProxy pxBench{};
            pxBench.kind = GardenStaticPhysicsProxyKind::TriangleMeshFromMeshBytes;
            pxBench.orientedHalfExtents = half;
            pxBench.orientedYaw = yawR;
            pxBench.orientedPitch = pitchR;
            pxBench.orientedRoll = rollR;
            push(
                candidate,
                0.88f,
                GardenColliderKind::HeroPlayground,
                GardenPropMesh::MeshBenchSlats,
                yawR,
                pitchR,
                rollR,
                pxBench);
            std::vector<std::uint8_t> baked;
            if (marble::core::meshAssetV1BakeGardenPropInstanceToWorldBytes(
                    std::span<std::uint8_t const>(kGardenBenchTemplate.data(), kGardenBenchTemplate.size()),
                    math::Vec3{c.x, c.y, c.z},
                    math::Vec3{half.x, half.y, half.z},
                    yawR,
                    pitchR,
                    rollR,
                    baked)) {
                out.propCollisionMeshBytes.back() = std::move(baked);
            } else {
                out.staticPhysicsProxies.back().kind = GardenStaticPhysicsProxyKind::AabbBox;
                out.propCollisionMeshBytes.back().clear();
            }
            placedBench = true;
        }
    }

    // Eight immovable boulders (~human-scale), partial bury into the nominal plane.
    int constexpr kBoulderCount = 8;
    for (int b = 0; b < kBoulderCount; ++b) {
        int tries = 0;
        while (tries < 220) {
            ++tries;
            float const t = uBoulderAngle(rngBoulder);
            float const rad = 28.f + uBoulder01(rngBoulder) * (R - 38.f);
            float const wx = std::cos(t) * rad;
            float const wz = std::sin(t) * rad;
            float hx = 0.62f + uBoulder01(rngBoulder) * 0.55f;
            float hy = 0.72f + uBoulder01(rngBoulder) * 0.62f;
            float hz = 0.58f + uBoulder01(rngBoulder) * 0.52f;
            float const bury = 0.18f + uBoulder01(rngBoulder) * 0.22f;
            float const ty = gardenTerrainHeightForStaticPlacement(out.terrain, wx, wz);
            Vec3 half{hx, hy, hz};
            int const pick = rockMeshPickBoulder(rngBoulder);
            GardenPropMesh const rm =
                pick < 5 ? GardenPropMesh::Cube : kGardenProcSphereRoughRockMesh;
            math::Vec3 const nB = gardenTerrainUpNormal(out.terrain, wx, wz);
            float constexpr kBoulderRockAlign = 0.45f;
            float yawR{};
            float pitchR{};
            float rollR{};
            Vec3 c{wx, ty + hy, wz};
            if (rm == GardenPropMesh::Cube) {
                gardenRockRestYprFromNormal(nB, uBoulderAngle(rngBoulder), kBoulderRockAlign, yawR, pitchR, rollR);
                c.y = ty + half.y + kGardenDecorSitClearanceM;
                gardenOrientedUnitBoxSitOnTerrainSupportSamples(
                    out.terrain, c, half, yawR, pitchR, rollR, kGardenDecorSitClearanceM);
                gardenApplyBuryThenClampOrientedBoxOnTerrain(
                    out.terrain,
                    c,
                    half,
                    yawR,
                    pitchR,
                    rollR,
                    bury,
                    kGardenDecorSitClearanceM);
            } else {
                float const sphereR = std::min({half.x, half.y, half.z}) * kRockSphereProxyInscribedScale;
                yawR = uBoulderAngle(rngBoulder);
                // Faceted mesh: asymmetric pitch/roll so silhouettes don't read bolt-upright/sphere-pin.
                // Physics proxy stays inscribed sphere—mesh underside can mismatch; tilt only masks silhouette.
                float constexpr kBoulderFacetedPitchRad = 0.22f;
                float constexpr kBoulderFacetedRollRad = 0.30f;
                pitchR = (uBoulder01(rngBoulder) - 0.5f) * 2.f * kBoulderFacetedPitchRad;
                rollR = (uBoulder01(rngBoulder) - 0.5f) * 2.f * kBoulderFacetedRollRad;
                c.y = ty + sphereR + kGardenDecorSitClearanceM;
                gardenSitSphereOnTerrain(
                    out.terrain, wx, wz, c.y, sphereR, kGardenDecorSitClearanceM);
                gardenApplyBuryThenClampSphereOnTerrain(
                    out.terrain, wx, wz, c.y, sphereR, bury, kGardenDecorSitClearanceM);
                gardenOrientedUnitBoxSitOnTerrainSupportSamples(
                    out.terrain, c, half, yawR, pitchR, rollR, kGardenDecorSitClearanceM);
            }
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawR, pitchR, rollR);
            if (overlapsAny(candidate, kBoulderPad)) {
                continue;
            }
            GardenStaticPhysicsProxy px{};
            if (rm == GardenPropMesh::Cube) {
                px.kind = GardenStaticPhysicsProxyKind::OrientedBox;
                px.orientedHalfExtents = half;
                px.orientedYaw = yawR;
                px.orientedPitch = pitchR;
                px.orientedRoll = rollR;
            } else {
                px.kind = GardenStaticPhysicsProxyKind::Sphere;
                px.sphereRadius = std::min({half.x, half.y, half.z}) * kRockSphereProxyInscribedScale;
            }
            push(candidate, 0.9f, GardenColliderKind::Boulder, rm, yawR, pitchR, rollR, px);
            break;
        }
    }

    // Logs: spawn high, then log-only bake settles against all static geometry.
    float constexpr kDropYMin = 14.f;
    float constexpr kDropYSpan = 16.f;
    int constexpr kTargetLogs = 12;
    for (int n = 0; n < kTargetLogs; ++n) {
        for (int tries = 0; tries < 260; ++tries) {
            float const ang = uLogAngle(rngLog);
            float const rad = uLogRadDist(rngLog);
            bool const alongX = uLog01(rngLog) > 0.5f;
            float const spanLong = 2.1f + uLog01(rngLog) * 2.4f;
            // Trunk-like cross section (~tree trunkR 0.11–0.20 m); symmetric so the capsule stays round.
            float const spanCross = 0.20f + uLog01(rngLog) * 0.16f;
            float hx = alongX ? spanLong * 0.5f : spanCross * 0.5f;
            float hy = spanCross * 0.5f;
            float hz = alongX ? spanCross * 0.5f : spanLong * 0.5f;
            float const y = kDropYMin + uLog01(rngLog) * kDropYSpan;
            float const wxL = std::cos(ang) * rad;
            float const wzL = std::sin(ang) * rad;
            Vec3 c{wxL, y, wzL};
            float yawL{};
            float pitchL{};
            float rollL{};
            gardenFallenCapsuleRestYpr(out.terrain, wxL, wzL, hx, hy, hz, uLogAngle(rngLog), yawL, pitchL, rollL);
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, Vec3{hx, hy, hz}, yawL, pitchL, rollL);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            push(
                candidate,
                0.68f,
                GardenColliderKind::Log,
                GardenPropMesh::TrunkY,
                yawL,
                pitchL,
                rollL,
                twigOrLogCapsuleProxy(hx, hy, hz, yawL, pitchL, rollL));
            break;
        }
    }

    settleGardenDebrisInPlace(out);

    // Small rocks and twigs: static scatter on/near the yard plane.
    for (int r = 0; r < 42; ++r) {
        for (int tries = 0; tries < 380; ++tries) {
            float const ang = uScatterAngle(rngScatter);
            float const rad = 12.f + uScatter01(rngScatter) * (R - 16.f);
            float const wx = std::cos(ang) * rad;
            float const wz = std::sin(ang) * rad;
            float const sx = 0.14f + uScatter01(rngScatter) * 0.38f;
            float const sy = 0.12f + uScatter01(rngScatter) * 0.32f;
            float const sz = 0.14f + uScatter01(rngScatter) * 0.36f;
            Vec3 half{sx * 0.5f, sy * 0.5f, sz * 0.5f};
            float const ty = gardenTerrainHeightForStaticPlacement(out.terrain, wx, wz);
            float const tuckScatter = gardenDecorEmbedTuckM(wx, wz, seed, static_cast<std::uint32_t>(r) * 834u + 0x71E602u);
            float const y = ty + half.y + kGardenDecorSitClearanceM;
            Vec3 c{wx, y, wz};
            int const pick = rockMeshPickScatter(rngScatter);
            GardenPropMesh const rm =
                pick < 5 ? GardenPropMesh::Cube : kGardenProcSphereRoughRockMesh;
            math::Vec3 const nR = gardenTerrainUpNormal(out.terrain, wx, wz);
            float constexpr kScatterRockAlign = 0.40f;
            float yawR{};
            float pitchR{};
            float rollR{};
            if (rm == GardenPropMesh::Cube) {
                gardenRockRestYprFromNormal(nR, uScatterAngle(rngScatter), kScatterRockAlign, yawR, pitchR, rollR);
            } else {
                yawR = uScatterAngle(rngScatter);
                pitchR = 0.f;
                rollR = 0.f;
            }
            if (rm == GardenPropMesh::Cube) {
                gardenOrientedUnitBoxSitOnTerrainSupportSamples(
                    out.terrain, c, half, yawR, pitchR, rollR, kGardenDecorSitClearanceM);
                gardenApplyBuryThenClampOrientedBoxOnTerrain(
                    out.terrain,
                    c,
                    half,
                    yawR,
                    pitchR,
                    rollR,
                    tuckScatter,
                    kGardenDecorSitClearanceM);
            } else {
                float const sphereR = std::min({half.x, half.y, half.z}) * kRockSphereProxyInscribedScale;
                c.y = ty + sphereR + kGardenDecorSitClearanceM;
                gardenSitSphereOnTerrain(out.terrain, wx, wz, c.y, sphereR, kGardenDecorSitClearanceM);
                gardenApplyBuryThenClampSphereOnTerrain(
                    out.terrain, wx, wz, c.y, sphereR, tuckScatter, kGardenDecorSitClearanceM);
                gardenOrientedUnitBoxSitOnTerrainSupportSamples(
                    out.terrain, c, half, yawR, pitchR, rollR, kGardenDecorSitClearanceM);
            }
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawR, pitchR, rollR);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            GardenStaticPhysicsProxy pxR{};
            if (rm == GardenPropMesh::Cube) {
                pxR.kind = GardenStaticPhysicsProxyKind::OrientedBox;
                pxR.orientedHalfExtents = half;
                pxR.orientedYaw = yawR;
                pxR.orientedPitch = pitchR;
                pxR.orientedRoll = rollR;
            } else {
                pxR.kind = GardenStaticPhysicsProxyKind::Sphere;
                pxR.sphereRadius = std::min({half.x, half.y, half.z}) * kRockSphereProxyInscribedScale;
            }
            push(candidate, 0.88f, GardenColliderKind::Rock, rm, yawR, pitchR, rollR, pxR);
            break;
        }
    }

    for (int n = 0; n < 38; ++n) {
        for (int tries = 0; tries < 320; ++tries) {
            float const ang = uTwigAngle(rngTwig);
            float const rad = 11.f + uTwig01(rngTwig) * (R - 15.f);
            bool const alongX = uTwig01(rngTwig) > 0.5f;
            float const spanLong = 0.38f + uTwig01(rngTwig) * 0.72f;
            float const spanMid = 0.03f + uTwig01(rngTwig) * 0.045f;
            float const spanShort = 0.028f + uTwig01(rngTwig) * 0.05f;
            float hx = alongX ? spanLong * 0.5f : spanMid * 0.5f;
            float hy = spanMid * 0.5f;
            float hz = alongX ? spanShort * 0.5f : spanLong * 0.5f;
            float const tx = std::cos(ang) * rad;
            float const tz = std::sin(ang) * rad;
            float const ty = gardenTerrainHeightForStaticPlacement(out.terrain, tx, tz);
            float const y = ty + hy + kGardenDecorSitClearanceM;
            Vec3 c{tx, y, tz};
            float yawTwig{};
            float pitchTw{};
            float rollTw{};
            gardenFallenCapsuleRestYpr(out.terrain, tx, tz, hx, hy, hz, uTwigAngle(rngTwig), yawTwig, pitchTw, rollTw);
            Vec3 const twigHalf{hx, hy, hz};
            gardenOrientedUnitBoxSitOnTerrainSupportSamples(
                out.terrain, c, twigHalf, yawTwig, pitchTw, rollTw, kGardenDecorSitClearanceM);
            GardenStaticPhysicsProxy const pxTwig =
                twigOrLogCapsuleProxy(hx, hy, hz, yawTwig, pitchTw, rollTw);
            gardenSnapCapsuleCenterOnAnalyticTerrain(
                out.terrain,
                c,
                pxTwig.capsuleHalfHeight,
                pxTwig.capsuleRadius,
                yawTwig,
                pitchTw,
                rollTw,
                pxTwig.intrinsicCylinderAxis,
                kGardenDecorSitClearanceM);
            gardenCapsuleRelaxCenterYOnTerrain(
                out.terrain,
                c,
                pxTwig.capsuleHalfHeight,
                pxTwig.capsuleRadius,
                yawTwig,
                pitchTw,
                rollTw,
                pxTwig.intrinsicCylinderAxis,
                kGardenDecorSitClearanceM);
            float const tuckTwig = gardenDecorEmbedTuckM(tx, tz, seed, static_cast<std::uint32_t>(n) * 734u + 0x813F04Bu);
            c.y -= tuckTwig;
            gardenSnapCapsuleCenterOnAnalyticTerrain(
                out.terrain,
                c,
                pxTwig.capsuleHalfHeight,
                pxTwig.capsuleRadius,
                yawTwig,
                pitchTw,
                rollTw,
                pxTwig.intrinsicCylinderAxis,
                kGardenDecorSitClearanceM);
            gardenCapsuleRelaxCenterYOnTerrain(
                out.terrain,
                c,
                pxTwig.capsuleHalfHeight,
                pxTwig.capsuleRadius,
                yawTwig,
                pitchTw,
                rollTw,
                pxTwig.intrinsicCylinderAxis,
                kGardenDecorSitClearanceM);
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, Vec3{hx, hy, hz}, yawTwig, pitchTw, rollTw);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            push(
                candidate,
                0.72f,
                GardenColliderKind::Twig,
                GardenPropMesh::TwigCapsule,
                yawTwig,
                pitchTw,
                rollTw,
                twigOrLogCapsuleProxy(hx, hy, hz, yawTwig, pitchTw, rollTw));
            break;
        }
    }

    int constexpr kTargetTrees = 62;
    for (int t = 0; t < kTargetTrees; ++t) {
        for (int tries = 0; tries < 220; ++tries) {
            float const ang = uTreeAngle(rngTree);
            float const rad = uTreeRadial(rngTree);
            float const wx = std::cos(ang) * rad;
            float const wz = std::sin(ang) * rad;
            float const slope = gardenTerrainSlopeMagnitude(out.terrain, wx, wz);
            if (slope > 0.42f) {
                continue;
            }
            float const ty = gardenTerrainHeightForStaticPlacement(out.terrain, wx, wz);
            float const tuckTree = gardenDecorEmbedTuckM(wx, wz, seed, static_cast<std::uint32_t>(t) * 917u + 0x924E15Cu);
            float const trunkH = 2.4f + uTree01(rngTree) * 3.8f;
            float const trunkR = 0.11f + uTree01(rngTree) * 0.09f;
            Vec3 const trunkCenter{wx, ty + trunkH * 0.5f - tuckTree, wz};
            Vec3 const trunkHalf{trunkR, trunkH * 0.5f, trunkR};
            float const yawT = uTreeAngle(rngTree);
            float const pitchT = (uTree01(rngTree) - 0.5f) * 0.12f;
            float const rollT = (uTree01(rngTree) - 0.5f) * 0.12f;
            Aabb const trunkBox = worldAabbOfOrientedUnitBox(trunkCenter, trunkHalf, yawT, pitchT, rollT);

            float const canopyR = 1.15f + uTree01(rngTree) * 1.35f;
            float const canopyLift = trunkH * 0.52f + uTree01(rngTree) * 0.35f * trunkH;
            Vec3 const canopyCenter{wx, ty + canopyLift - tuckTree, wz};
            Vec3 const canopyHalf{canopyR * 0.55f, canopyR * 0.42f, canopyR * 0.55f};
            float const yawC = uTreeAngle(rngTree);
            float const pitchC = (uTree01(rngTree) - 0.5f) * 0.35f;
            float const rollC = (uTree01(rngTree) - 0.5f) * 0.35f;
            Aabb const canopyBox = worldAabbOfOrientedUnitBox(canopyCenter, canopyHalf, yawC, pitchC, rollC);
            Aabb treeUnion{
                {
                    std::min(trunkBox.min.x, canopyBox.min.x),
                    std::min(trunkBox.min.y, canopyBox.min.y),
                    std::min(trunkBox.min.z, canopyBox.min.z),
                },
                {
                    std::max(trunkBox.max.x, canopyBox.max.x),
                    std::max(trunkBox.max.y, canopyBox.max.y),
                    std::max(trunkBox.max.z, canopyBox.max.z),
                },
            };
            if (overlapsAny(treeUnion, kPropPad)) {
                continue;
            }
            push(
                trunkBox,
                0.62f,
                GardenColliderKind::TreeTrunk,
                GardenPropMesh::TrunkY,
                yawT,
                pitchT,
                rollT,
                trunkCapsuleProxy(trunkHalf.y, trunkR, yawT, pitchT, rollT));
            GardenStaticPhysicsProxy pxCanopy{};
            pxCanopy.kind = GardenStaticPhysicsProxyKind::Sphere;
            pxCanopy.sphereRadius =
                std::min({canopyHalf.x, canopyHalf.y, canopyHalf.z}) * kFoliageSphereProxyScale;
            push(canopyBox, 0.78f, GardenColliderKind::TreeFoliage, GardenPropMesh::Foliage, yawC, pitchC, rollC, pxCanopy);
            break;
        }
    }

    int constexpr kUnderstory = 48;
    for (int u = 0; u < kUnderstory; ++u) {
        for (int tries = 0; tries < 200; ++tries) {
            float const ang = uUnderAngle(rngUnder);
            float const rad = 10.f + uUnder01(rngUnder) * (R - 18.f);
            float const wx = std::cos(ang) * rad;
            float const wz = std::sin(ang) * rad;
            if (gardenTerrainSlopeMagnitude(out.terrain, wx, wz) > 0.5f) {
                continue;
            }
            float const ty = gardenTerrainHeightForStaticPlacement(out.terrain, wx, wz);
            float const hr = 0.08f + gardenUnderstoryLeafSpanU01(seed, u) * 0.14f;
            Vec3 half{hr, hr * 0.35f, hr};
            float const rLeaf = std::min(std::min(half.x, half.y), half.z) * kRockSphereProxyInscribedScale;
            Vec3 c{wx, ty, wz};
            gardenSitSphereOnTerrain(out.terrain, wx, wz, c.y, rLeaf, kGardenDecorSitClearanceM);
            float const tuckLeaf =
                gardenDecorEmbedTuckM(wx, wz, seed, static_cast<std::uint32_t>(u) * 661u + 0xAF1005Du);
            gardenApplyBuryThenClampSphereOnTerrain(
                out.terrain,
                wx,
                wz,
                c.y,
                rLeaf,
                tuckLeaf,
                kGardenDecorSitClearanceM);
            math::Vec3 const nLeaf = gardenTerrainUpNormal(out.terrain, wx, wz);
            float yawL{};
            float pitchL{};
            float rollL{};
            float constexpr kLeafRockAlign = 0.22f;
            gardenRockRestYprFromNormal(nLeaf, uUnderAngle(rngUnder), kLeafRockAlign, yawL, pitchL, rollL);
            gardenOrientedUnitBoxSitOnTerrainSupportSamples(
                out.terrain, c, half, yawL, pitchL, rollL, kGardenDecorSitClearanceM);
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawL, pitchL, rollL);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            GardenStaticPhysicsProxy pxLeaf{};
            pxLeaf.kind = GardenStaticPhysicsProxyKind::Sphere;
            pxLeaf.sphereRadius = std::min({half.x, half.y, half.z}) * kRockSphereProxyInscribedScale;
            push(candidate, 0.85f, GardenColliderKind::Leaf, GardenPropMesh::Icosahedron, yawL, pitchL, rollL, pxLeaf);
            break;
        }
    }
}

void settleGardenDebrisInPlace(GardenLayout& layout) noexcept {
    std::vector<std::size_t> debris{};
    std::size_t const n = layout.kinds.size();
    debris.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (isLogBakeKind(layout.kinds[i])) {
            debris.push_back(i);
        }
    }
    if (debris.empty()) {
        return;
    }

    constexpr float dt = 1.f / 100.f;
    constexpr int kMaxSteps = 2400;
    constexpr int kInnerIters = 8;
    constexpr float kRest = 0.f;
    constexpr float kStaticTangent = 0.38f;
    constexpr float kPairTangent = 0.38f;
    constexpr float kSleepSpeed = 0.02f;
    constexpr int kMinSleepStep = 400;

    std::vector<math::Vec3> ori(debris.size(), math::Vec3::zero());
    std::vector<math::Vec3> vel(debris.size(), math::Vec3::zero());

    auto refreshCollFromOrientedMeshCenter = [&](std::size_t kSlot, std::size_t idx, Vec3 const& centerMesh) noexcept {
        GardenStaticPhysicsProxy const& px =
            idx < layout.staticPhysicsProxies.size() ? layout.staticPhysicsProxies[idx] : GardenStaticPhysicsProxy{};
        float const yaw =
            idx < layout.propYaw.size() ? layout.propYaw[idx] : 0.f;
        float const pitch =
            idx < layout.propPitch.size() ? layout.propPitch[idx] : 0.f;
        float const roll =
            idx < layout.propRoll.size() ? layout.propRoll[idx] : 0.f;
        Vec3 half = logTrunkOrientedHalfFromProxy(px);
        layout.staticColliders[idx] = worldAabbOfOrientedUnitBox(centerMesh, half, yaw, pitch, roll);
        (void)kSlot;
    };

    for (std::size_t k = 0; k < debris.size(); ++k) {
        std::size_t const idx = debris[k];
        GardenStaticPhysicsProxy const& px =
            idx < layout.staticPhysicsProxies.size() ? layout.staticPhysicsProxies[idx] : GardenStaticPhysicsProxy{};
        Vec3 c = (layout.staticColliders[idx].min + layout.staticColliders[idx].max) * 0.5f;
        if (px.kind == GardenStaticPhysicsProxyKind::Capsule) {
            float const yaw =
                idx < layout.propYaw.size() ? layout.propYaw[idx] : 0.f;
            float const pitch =
                idx < layout.propPitch.size() ? layout.propPitch[idx] : 0.f;
            float const roll =
                idx < layout.propRoll.size() ? layout.propRoll[idx] : 0.f;
            (void)refineLogOrientedCenterFromCollider(px, yaw, pitch, roll, layout.staticColliders[idx], c);
        }
        ori[k] = c;
        refreshCollFromOrientedMeshCenter(k, idx, ori[k]);
    }

    math::Vec3 const g{0.f, -9.81f, 0.f};

    for (int step = 0; step < kMaxSteps; ++step) {
        for (std::size_t k = 0; k < debris.size(); ++k) {
            std::size_t const idx = debris[k];
            vel[k] = vel[k] + g * dt;
            ori[k] = ori[k] + vel[k] * dt;
            refreshCollFromOrientedMeshCenter(k, idx, ori[k]);
        }

        for (int it = 0; it < kInnerIters; ++it) {
            for (std::size_t k = 0; k < debris.size(); ++k) {
                std::size_t const idx = debris[k];
                math::Aabb dynBox = layout.staticColliders[idx];
                Vec3 c = (dynBox.min + dynBox.max) * 0.5f;
                Vec3 const h = (dynBox.max - dynBox.min) * 0.5f;
                Vec3 const cStart{c.x, c.y, c.z};
                for (std::size_t j = 0; j < n; ++j) {
                    if (j == idx || isLogBakeKind(layout.kinds[j])) {
                        continue;
                    }
                    GardenColliderKind const jKind =
                        j < layout.kinds.size() ? layout.kinds[j] : GardenColliderKind::GrassBump;
                    GardenStaticPhysicsProxy const& jpx =
                        j < layout.staticPhysicsProxies.size() ? layout.staticPhysicsProxies[j] : GardenStaticPhysicsProxy{};
                    bool done = false;
                    if (jKind == GardenColliderKind::Boulder && jpx.kind == GardenStaticPhysicsProxyKind::Sphere) {
                        float const sr = std::max(jpx.sphereRadius, 1.0e-4f);
                        Vec3 const sc{(layout.staticColliders[j].min + layout.staticColliders[j].max) * 0.5f};
                        done =
                            separateMovingAxisAabbFromStaticSphere(c, h, vel[k], sc, sr, kStaticTangent, kRest);
                    }
                    if (!done) {
                        (void)separateDebrisFromStatic(c, h, vel[k], layout.staticColliders[j], kStaticTangent, kRest);
                    }
                }
                ori[k] = ori[k] + Vec3{c.x - cStart.x, c.y - cStart.y, c.z - cStart.z};
                refreshCollFromOrientedMeshCenter(k, idx, ori[k]);
            }

            for (std::size_t aa = 0; aa < debris.size(); ++aa) {
                for (std::size_t bi = aa + 1; bi < debris.size(); ++bi) {
                    std::size_t const ia = debris[aa];
                    std::size_t const ib = debris[bi];
                    math::Aabb& ba = layout.staticColliders[ia];
                    math::Aabb& bb = layout.staticColliders[ib];
                    math::Vec3 ca = (ba.min + ba.max) * 0.5f;
                    Vec3 ca0{ca.x, ca.y, ca.z};
                    math::Vec3 ha = (ba.max - ba.min) * 0.5f;
                    math::Vec3 cb = (bb.min + bb.max) * 0.5f;
                    Vec3 cb0{cb.x, cb.y, cb.z};
                    math::Vec3 hb = (bb.max - bb.min) * 0.5f;
                    float const ra = std::max(ha.x, std::max(ha.y, ha.z));
                    float const rb = std::max(hb.x, std::max(hb.y, hb.z));
                    Vec3 const d = ca - cb;
                    float const reach = ra + rb + 0.55f;
                    if (math::lengthSquared(d) > reach * reach) {
                        continue;
                    }
                    float const invA = gardenLooseBodyInverseMass(layout.kinds[ia]);
                    float const invB = gardenLooseBodyInverseMass(layout.kinds[ib]);
                    separateDebrisPairWeighted(ca, ha, vel[aa], invA, cb, hb, vel[bi], invB, kRest, kPairTangent);
                    ori[aa] = ori[aa] +
                        Vec3{ca.x - ca0.x, ca.y - ca0.y, ca.z - ca0.z};
                    ori[bi] = ori[bi] +
                        Vec3{cb.x - cb0.x, cb.y - cb0.y, cb.z - cb0.z};
                    refreshCollFromOrientedMeshCenter(aa, ia, ori[aa]);
                    refreshCollFromOrientedMeshCenter(bi, ib, ori[bi]);
                }
            }
        }

        for (std::size_t k = 0; k < debris.size(); ++k) {
            std::size_t const idx = debris[k];
            GardenStaticPhysicsProxy const& px =
                idx < layout.staticPhysicsProxies.size() ? layout.staticPhysicsProxies[idx] : GardenStaticPhysicsProxy{};
            if (px.kind != GardenStaticPhysicsProxyKind::Capsule) {
                continue;
            }
            Vec3 cen = ori[k];
            math::Vec3 meshAxis{0.f, 1.f, 0.f};
            if (px.intrinsicCylinderAxis == 1u) {
                meshAxis = {1.f, 0.f, 0.f};
            } else if (px.intrinsicCylinderAxis == 2u) {
                meshAxis = {0.f, 0.f, 1.f};
            }
            float const yawL = idx < layout.propYaw.size() ? layout.propYaw[idx] : 0.f;
            float const pitchL = idx < layout.propPitch.size() ? layout.propPitch[idx] : 0.f;
            float const rollL = idx < layout.propRoll.size() ? layout.propRoll[idx] : 0.f;
            math::Mat4 const rot = math::Mat4::rotationY(yawL) * math::Mat4::rotationX(pitchL) *
                math::Mat4::rotationZ(rollL);
            math::Vec3 const wAxis = marble::math::transformDirection(rot, meshAxis);
            float const hh = std::max(px.capsuleHalfHeight, 1.0e-4f);
            float const wr = std::max(px.capsuleRadius, 1.0e-4f);
            float const pen =
                gardenCapsuleMaxTerrainClearanceDeficit(
                    layout.terrain, cen, wAxis, hh, wr, kGardenDecorSitClearanceM);
            if (pen > 1.0e-4f) {
                cen.y += pen;
                ori[k].y += pen;
                if (vel[k].y < 0.f) {
                    vel[k].y = 0.f;
                }
                refreshCollFromOrientedMeshCenter(k, idx, ori[k]);
            }
        }

        if (step >= kMinSleepStep) {
            float vmax = 0.f;
            for (math::Vec3 const& v : vel) {
                vmax = std::max(vmax, std::sqrt(math::lengthSquared(v)));
            }
            if (vmax < kSleepSpeed) {
                break;
            }
        }
    }

    for (std::size_t k = 0; k < debris.size(); ++k) {
        std::size_t const idx = debris[k];
        snapLogColliderToTerrainFromCapsuleLayout(layout, idx);
    }
}

void placeMarblesInArena(std::array<physics::RigidBodyKinematics, 2>& marbles, GardenLayout const& layout) noexcept {
    marbles[0] = {};
    marbles[1] = {};
    float const invMass = 1.f / kPlayerBallMassKg;
    marbles[0].invMass = invMass;
    marbles[1].invMass = invMass;
    float const halfSep = kArenaRadius * 0.35f;
    // See [`kGardenMarbleSpawnClearanceAboveTerrainM`] in GardenSimulation.hpp (Jolt vs analytic terrain).
    float const y0 =
        gardenTerrainHeightLayoutMeters(layout.terrain, -halfSep, 0.f) + kMarbleRadius +
        kGardenMarbleSpawnClearanceAboveTerrainM;
    float const y1 =
        gardenTerrainHeightLayoutMeters(layout.terrain, halfSep, 0.f) + kMarbleRadius +
        kGardenMarbleSpawnClearanceAboveTerrainM;
    marbles[0].position = {-halfSep, y0, 0.f};
    marbles[1].position = {halfSep, y1, 0.f};
}

bool gardenBallOnGround(
    GardenLayout const& layout,
    physics::RigidBodyKinematics const& ball,
    float radius
) noexcept {
    if (ball.invMass <= 0.f) {
        return false;
    }
    if (ball.linearVelocity.y > kGardenBallOnGroundMaxUpwardVyMps) {
        return false;
    }
    float const px = ball.position.x;
    float const pz = ball.position.z;
    float const d = radius * kGardenBallOnGroundFootprintScale;
    GardenTerrain const& tr = layout.terrain;
    float tyMax = gardenTerrainHeightAt(tr, px, pz);
    tyMax = std::max(tyMax, gardenTerrainHeightAt(tr, px + d, pz));
    tyMax = std::max(tyMax, gardenTerrainHeightAt(tr, px - d, pz));
    tyMax = std::max(tyMax, gardenTerrainHeightAt(tr, px, pz + d));
    tyMax = std::max(tyMax, gardenTerrainHeightAt(tr, px, pz - d));
    float const bottom = ball.position.y - radius;
    return bottom <= tyMax + kGardenBallOnGroundClearanceM;
}

namespace {

[[nodiscard]] float jumpImpulseEase(float t) noexcept {
    t = std::clamp(t, 0.f, 1.f);
    constexpr float p1x = 0.26f;
    constexpr float p1y = 0.02f;
    constexpr float p2x = 0.72f;
    constexpr float p2y = 0.985f;
    auto bezierX = [](float u) -> float {
        float const o = 1.f - u;
        return 3.f * o * o * u * p1x + 3.f * o * u * u * p2x + u * u * u;
    };
    auto bezierY = [](float u) -> float {
        float const o = 1.f - u;
        return 3.f * o * o * u * p1y + 3.f * o * u * u * p2y + u * u * u;
    };
    float lo = 0.f;
    float hi = 1.f;
    for (int i = 0; i < 16; ++i) {
        float const mid = 0.5f * (lo + hi);
        if (bezierX(mid) < t) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    float const u = 0.5f * (lo + hi);
    return bezierY(u);
}

} // namespace

float gardenJumpImpulseFromHoldSeconds(float holdSeconds) noexcept {
    float const t = std::clamp(holdSeconds / kGardenJumpChargeMaxSec, 0.f, 1.f);
    float const s = jumpImpulseEase(t);
    return kGardenJumpImpulseMin + (kGardenJumpImpulseMax - kGardenJumpImpulseMin) * s;
}

bool rayIntersectHorizontalPlane(
    Vec3 rayOrigin,
    Vec3 rayDir,
    float planeY,
    Vec3& outPoint
) noexcept {
    float const dy = rayDir.y;
    if (std::fabs(dy) < 1e-6f) {
        return false;
    }
    float const t = (planeY - rayOrigin.y) / dy;
    if (t < 0.f) {
        return false;
    }
    outPoint = rayOrigin + rayDir * t;
    return true;
}

bool rayHitsSphere(
    Vec3 rayOrigin,
    Vec3 rayDir,
    Vec3 sphereCenter,
    float sphereRadius,
    float& outT
) noexcept {
    Vec3 const oc = rayOrigin - sphereCenter;
    float const a = math::dot(rayDir, rayDir);
    if (a < 1e-12f) {
        return false;
    }
    float const halfB = math::dot(oc, rayDir);
    float const c = math::dot(oc, oc) - sphereRadius * sphereRadius;
    float const disc = halfB * halfB - a * c;
    if (disc < 0.f) {
        return false;
    }
    float const s = std::sqrt(disc);
    float const t0 = (-halfB - s) / a;
    float const t1 = (-halfB + s) / a;
    float tHit = (t0 >= 0.f) ? t0 : t1;
    if (tHit < 0.f) {
        tHit = (t1 >= 0.f) ? t1 : t0;
    }
    if (tHit < 0.f) {
        return false;
    }
    outT = tHit;
    return true;
}

namespace {

[[nodiscard]] math::Vec3 gameplayOffsetToBodyLocal(
    math::Vec3 const& deltaGameplay,
    float yaw,
    float pitch,
    float roll
) noexcept {
    math::Mat4 const r =
        math::Mat4::rotationY(yaw) * math::Mat4::rotationX(pitch) * math::Mat4::rotationZ(roll);
    return {
        r.m[0] * deltaGameplay.x + r.m[1] * deltaGameplay.y + r.m[2] * deltaGameplay.z,
        r.m[4] * deltaGameplay.x + r.m[5] * deltaGameplay.y + r.m[6] * deltaGameplay.z,
        r.m[8] * deltaGameplay.x + r.m[9] * deltaGameplay.y + r.m[10] * deltaGameplay.z,
    };
}

} // namespace

void gardenAddStaticPropBodiesFromLayout(
    marble::physics::IPhysicsScene& scene,
    GardenLayout const& layout,
    marble::gameplay::SimulationIsland const& island,
    marble::physics::PhysicsBodyMaterial const& propMat) noexcept {
    using marble::gameplay::localGameplayAabbToJolt;
    using marble::gameplay::localGameplayToJolt;

    static thread_local std::vector<math::Vec3> tHullSrc;
    static thread_local std::vector<math::Vec3> tHullBody;
    static thread_local std::vector<math::Vec3> tMeshVerts;
    static thread_local std::vector<math::Vec3> tMeshBody;
    static thread_local std::vector<std::uint32_t> tMeshIdx;

    std::size_t const n = layout.staticColliders.size();
    for (std::size_t i = 0; i < n; ++i) {
        GardenStaticPhysicsProxy const& px =
            i < layout.staticPhysicsProxies.size() ? layout.staticPhysicsProxies[i] : GardenStaticPhysicsProxy{};
        switch (px.kind) {
        case GardenStaticPhysicsProxyKind::AabbBox: {
            marble::physics::PhysicsStaticBoxDesc d{};
            d.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
            d.material = propMat;
            (void)scene.addStaticBox(d);
            break;
        }
        case GardenStaticPhysicsProxyKind::Sphere: {
            math::Aabb const& b = layout.staticColliders[i];
            math::Vec3 const ctr = (b.min + b.max) * 0.5f;
            marble::physics::PhysicsStaticSphereDesc s{};
            s.center = localGameplayToJolt(island, ctr);
            s.radius = std::max(px.sphereRadius, 1.0e-4f);
            s.material = propMat;
            (void)scene.addStaticSphere(s);
            break;
        }
        case GardenStaticPhysicsProxyKind::OrientedBox: {
            math::Aabb const& b = layout.staticColliders[i];
            math::Vec3 const ctr = (b.min + b.max) * 0.5f;
            marble::physics::PhysicsStaticOrientedBoxDesc ob{};
            ob.center = localGameplayToJolt(island, ctr);
            ob.halfExtents = px.orientedHalfExtents;
            ob.yawRadians = px.orientedYaw;
            ob.pitchRadians = px.orientedPitch;
            ob.rollRadians = px.orientedRoll;
            ob.material = propMat;
            (void)scene.addStaticOrientedBox(ob);
            break;
        }
        case GardenStaticPhysicsProxyKind::Capsule: {
            math::Aabb const& b = layout.staticColliders[i];
            math::Vec3 const ctr = (b.min + b.max) * 0.5f;
            marble::physics::PhysicsStaticCapsuleDesc cap{};
            cap.center = localGameplayToJolt(island, ctr);
            cap.halfHeight = std::max(px.capsuleHalfHeight, 1.0e-4f);
            cap.radius = std::max(px.capsuleRadius, 1.0e-4f);
            cap.yawRadians = px.capsuleYaw;
            cap.pitchRadians = px.capsulePitch;
            cap.rollRadians = px.capsuleRoll;
            cap.intrinsicCylinderAxis = px.intrinsicCylinderAxis;
            cap.material = propMat;
            (void)scene.addStaticCapsule(cap);
            break;
        }
        case GardenStaticPhysicsProxyKind::ConvexHullFromMeshBytes:
        case GardenStaticPhysicsProxyKind::TriangleMeshFromMeshBytes: {
            bool const wantHull = px.kind == GardenStaticPhysicsProxyKind::ConvexHullFromMeshBytes;
            std::span<std::uint8_t const> bytes{};
            if (i < layout.propCollisionMeshBytes.size()) {
                std::vector<std::uint8_t> const& slot = layout.propCollisionMeshBytes[i];
                if (!slot.empty()) {
                    bytes = std::span<std::uint8_t const>(slot.data(), slot.size());
                }
            }
            std::optional<marble::core::MeshAssetV1CpuViews> const parsed = marble::core::meshAssetV1TryParse(bytes);
            if (!parsed.has_value()) {
                marble::physics::PhysicsStaticBoxDesc d{};
                d.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
                d.material = propMat;
                (void)scene.addStaticBox(d);
                break;
            }
            math::Aabb const& b = layout.staticColliders[i];
            math::Vec3 const ctr = (b.min + b.max) * 0.5f;
            float const yaw = i < layout.propYaw.size() ? layout.propYaw[i] : 0.f;
            float const pitch = i < layout.propPitch.size() ? layout.propPitch[i] : 0.f;
            float const roll = i < layout.propRoll.size() ? layout.propRoll[i] : 0.f;

            marble::physics::PhysicsBodyId bid = marble::physics::kInvalidPhysicsBodyId;
            if (wantHull) {
                tHullSrc.clear();
                tHullBody.clear();
                if (!marble::core::meshAssetV1ConvexHullSourcePointsResampled(*parsed, tHullSrc) || tHullSrc.empty()) {
                    marble::physics::PhysicsStaticBoxDesc d{};
                    d.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
                    d.material = propMat;
                    (void)scene.addStaticBox(d);
                    break;
                }
                tHullBody.reserve(tHullSrc.size());
                for (math::Vec3 const& v : tHullSrc) {
                    math::Vec3 const d{v.x - ctr.x, v.y - ctr.y, v.z - ctr.z};
                    tHullBody.push_back(gameplayOffsetToBodyLocal(d, yaw, pitch, roll));
                }
                marble::physics::PhysicsStaticConvexHullDesc ch{};
                ch.center = localGameplayToJolt(island, ctr);
                ch.yawRadians = yaw;
                ch.pitchRadians = pitch;
                ch.rollRadians = roll;
                ch.points = std::span<math::Vec3 const>(tHullBody.data(), tHullBody.size());
                ch.material = propMat;
                bid = scene.addStaticConvexHull(ch);
            } else {
                tMeshVerts.clear();
                tMeshBody.clear();
                tMeshIdx.clear();
                if (!marble::core::meshAssetV1IndexedTriangleMeshForPhysics(*parsed, tMeshVerts, tMeshIdx) ||
                    tMeshVerts.empty() || tMeshIdx.empty()) {
                    marble::physics::PhysicsStaticBoxDesc d{};
                    d.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
                    d.material = propMat;
                    (void)scene.addStaticBox(d);
                    break;
                }
                tMeshBody.reserve(tMeshVerts.size());
                for (math::Vec3 const& v : tMeshVerts) {
                    math::Vec3 const d{v.x - ctr.x, v.y - ctr.y, v.z - ctr.z};
                    tMeshBody.push_back(gameplayOffsetToBodyLocal(d, yaw, pitch, roll));
                }
                marble::physics::PhysicsStaticTriangleMeshDesc tm{};
                tm.center = localGameplayToJolt(island, ctr);
                tm.yawRadians = yaw;
                tm.pitchRadians = pitch;
                tm.rollRadians = roll;
                tm.vertices = std::span<math::Vec3 const>(tMeshBody.data(), tMeshBody.size());
                tm.indices = std::span<std::uint32_t const>(tMeshIdx.data(), tMeshIdx.size());
                tm.enhancedInternalEdgeRemoval = true;
                tm.material = propMat;
                bid = scene.addStaticTriangleMesh(tm);
            }
            if (bid == marble::physics::kInvalidPhysicsBodyId) {
                marble::physics::PhysicsStaticBoxDesc d{};
                d.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
                d.material = propMat;
                (void)scene.addStaticBox(d);
            }
            break;
        }
        default:
            break;
        }
    }
}

} // namespace marble::garden
