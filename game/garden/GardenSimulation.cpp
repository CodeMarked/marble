#include "garden/GardenSimulation.hpp"

#include "math/Mat4.hpp"
#include "math/Vec3.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace marble::garden {

namespace {

using math::Aabb;
using math::Vec3;

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

[[nodiscard]] Vec3 clampToAabb(Vec3 p, Aabb const& b) noexcept {
    return {
        std::clamp(p.x, b.min.x, b.max.x),
        std::clamp(p.y, b.min.y, b.max.y),
        std::clamp(p.z, b.min.z, b.max.z),
    };
}

[[nodiscard]] bool aabbOverlap(Aabb const& a, Aabb const& b) noexcept {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y &&
        a.min.z <= b.max.z && a.max.z >= b.min.z;
}

/// Bodies that participate in load-time log settle (translation only); everything else is static for that phase.
[[nodiscard]] bool isLogBakeKind(GardenColliderKind k) noexcept {
    return k == GardenColliderKind::Log;
}

void writeAabbFromCenterHalf(Vec3 center, Vec3 half, Aabb& box) noexcept {
    box.min = center - half;
    box.max = center + half;
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

void buildGardenLayout(std::uint32_t seed, GardenLayout& out) noexcept {
    out.terrain = {};
    out.staticColliders.clear();
    out.kinds.clear();
    out.colliderTangentRetention.clear();
    out.propMesh.clear();
    out.propYaw.clear();
    out.propPitch.clear();
    out.propRoll.clear();

    float const R = kYardHalfExtent;
    fillGardenTerrain(seed, R, out.terrain);

    auto push = [&](Aabb const& box, float tangentRetention, GardenColliderKind kind,
                    GardenPropMesh mesh = GardenPropMesh::Cube, float yawRad = 0.f, float pitchRad = 0.f,
                    float rollRad = 0.f) {
        out.staticColliders.push_back(box);
        out.colliderTangentRetention.push_back(tangentRetention);
        out.kinds.push_back(kind);
        out.propMesh.push_back(static_cast<std::uint8_t>(mesh));
        out.propYaw.push_back(yawRad);
        out.propPitch.push_back(pitchRad);
        out.propRoll.push_back(rollRad);
    };

    std::mt19937 rng(seed ^ 0x51EDu);
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    std::uniform_real_distribution<float> uAngle(0.f, 6.2831853f);
    std::uniform_real_distribution<float> uRadial(22.f, R - 12.f);
    std::uniform_int_distribution<int> rockMeshPick(0, 9);

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
            float const groundY = gardenTerrainHeightAt(out.terrain, cx, cz);
            float const yBase = groundY + 0.04f;
            float const yCenter = yBase + courseH * 0.5f + static_cast<float>(row) * (courseH - 0.03f);
            Vec3 const center{cx, yCenter, cz};
            Vec3 half{brickW * 0.5f, courseH * 0.5f, brickD * 0.5f};
            push({center - half, center + half}, 0.84f, GardenColliderKind::Wall);
        }
    }

    // Eight immovable boulders (~human-scale), partial bury into the nominal plane.
    int constexpr kBoulderCount = 8;
    for (int b = 0; b < kBoulderCount; ++b) {
        int tries = 0;
        while (tries < 220) {
            ++tries;
            float const t = uAngle(rng);
            float const rad = 28.f + u01(rng) * (R - 38.f);
            float const wx = std::cos(t) * rad;
            float const wz = std::sin(t) * rad;
            float hx = 0.62f + u01(rng) * 0.55f;
            float hy = 0.72f + u01(rng) * 0.62f;
            float hz = 0.58f + u01(rng) * 0.52f;
            float const bury = 0.18f + u01(rng) * 0.22f;
            float const ty = gardenTerrainHeightAt(out.terrain, wx, wz);
            float const y = ty + hy - bury;
            Vec3 c{wx, y, wz};
            Vec3 half{hx, hy, hz};
            GardenPropMesh rm = GardenPropMesh::Icosahedron;
            int const pick = rockMeshPick(rng);
            if (pick <= 3) {
                rm = GardenPropMesh::Icosahedron;
            } else if (pick <= 8) {
                rm = GardenPropMesh::Octahedron;
            } else {
                rm = GardenPropMesh::Cube;
            }
            float const yawR = uAngle(rng);
            float const pitchR = (u01(rng) - 0.5f) * 0.45f;
            float const rollR = (u01(rng) - 0.5f) * 0.45f;
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawR, pitchR, rollR);
            if (overlapsAny(candidate, kBoulderPad)) {
                continue;
            }
            push(candidate, 0.9f, GardenColliderKind::Boulder, rm, yawR, pitchR, rollR);
            break;
        }
    }

    // Logs: spawn high, then log-only bake settles against all static geometry.
    float constexpr kDropYMin = 14.f;
    float constexpr kDropYSpan = 16.f;
    int constexpr kTargetLogs = 12;
    std::uniform_real_distribution<float> uLogRad(18.f, R - 14.f);
    for (int n = 0; n < kTargetLogs; ++n) {
        for (int tries = 0; tries < 260; ++tries) {
            float const ang = uAngle(rng);
            float const rad = uLogRad(rng);
            bool const alongX = u01(rng) > 0.5f;
            float const spanLong = 2.1f + u01(rng) * 2.4f;
            float const spanMid = 0.11f + u01(rng) * 0.14f;
            float const spanShort = 0.10f + u01(rng) * 0.12f;
            float hx = alongX ? spanLong * 0.5f : spanMid * 0.5f;
            float hy = spanMid * 0.5f;
            float hz = alongX ? spanShort * 0.5f : spanLong * 0.5f;
            float const y = kDropYMin + u01(rng) * kDropYSpan;
            Vec3 c{std::cos(ang) * rad, y, std::sin(ang) * rad};
            float const yawL = uAngle(rng) + (alongX ? 0.f : 1.5707963f);
            float const pitchL = (u01(rng) - 0.5f) * 0.38f;
            float const rollL = (u01(rng) - 0.5f) * 0.38f;
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, Vec3{hx, hy, hz}, yawL, pitchL, rollL);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            push(candidate, 0.68f, GardenColliderKind::Log, GardenPropMesh::TwigCapsule, yawL, pitchL, rollL);
            break;
        }
    }

    settleGardenDebrisInPlace(out);

    // Small rocks and twigs: static scatter on/near the yard plane.
    for (int r = 0; r < 42; ++r) {
        for (int tries = 0; tries < 380; ++tries) {
            float const ang = uAngle(rng);
            float const rad = 12.f + u01(rng) * (R - 16.f);
            float const wx = std::cos(ang) * rad;
            float const wz = std::sin(ang) * rad;
            float const sx = 0.14f + u01(rng) * 0.38f;
            float const sy = 0.12f + u01(rng) * 0.32f;
            float const sz = 0.14f + u01(rng) * 0.36f;
            Vec3 half{sx * 0.5f, sy * 0.5f, sz * 0.5f};
            float const ty = gardenTerrainHeightAt(out.terrain, wx, wz);
            float const y = ty + half.y + 0.004f;
            Vec3 c{wx, y, wz};
            GardenPropMesh rm = GardenPropMesh::Icosahedron;
            int const pick = rockMeshPick(rng);
            if (pick <= 3) {
                rm = GardenPropMesh::Icosahedron;
            } else if (pick <= 8) {
                rm = GardenPropMesh::Octahedron;
            } else {
                rm = GardenPropMesh::Cube;
            }
            float const yawR = uAngle(rng);
            float const pitchR = (u01(rng) - 0.5f) * 0.58f;
            float const rollR = (u01(rng) - 0.5f) * 0.58f;
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawR, pitchR, rollR);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            push(candidate, 0.88f, GardenColliderKind::Rock, rm, yawR, pitchR, rollR);
            break;
        }
    }

    for (int n = 0; n < 38; ++n) {
        for (int tries = 0; tries < 320; ++tries) {
            float const ang = uAngle(rng);
            float const rad = 11.f + u01(rng) * (R - 15.f);
            bool const alongX = u01(rng) > 0.5f;
            float const spanLong = 0.38f + u01(rng) * 0.72f;
            float const spanMid = 0.03f + u01(rng) * 0.045f;
            float const spanShort = 0.028f + u01(rng) * 0.05f;
            float hx = alongX ? spanLong * 0.5f : spanMid * 0.5f;
            float hy = spanMid * 0.5f;
            float hz = alongX ? spanShort * 0.5f : spanLong * 0.5f;
            float const tx = std::cos(ang) * rad;
            float const tz = std::sin(ang) * rad;
            float const ty = gardenTerrainHeightAt(out.terrain, tx, tz);
            float const y = ty + hy + 0.004f;
            Vec3 c{tx, y, tz};
            float const yawTwig = uAngle(rng) + (alongX ? 0.f : 1.5707963f);
            float const pitchTw = (u01(rng) - 0.5f) * 0.55f;
            float const rollTw = (u01(rng) - 0.5f) * 0.55f;
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
                rollTw
            );
            break;
        }
    }

    int constexpr kTargetTrees = 62;
    std::uniform_real_distribution<float> uTreeRad(14.f, R - 22.f);
    for (int t = 0; t < kTargetTrees; ++t) {
        for (int tries = 0; tries < 220; ++tries) {
            float const ang = uAngle(rng);
            float const rad = uTreeRad(rng);
            float const wx = std::cos(ang) * rad;
            float const wz = std::sin(ang) * rad;
            float const slope = gardenTerrainSlopeMagnitude(out.terrain, wx, wz);
            if (slope > 0.42f) {
                continue;
            }
            float const ty = gardenTerrainHeightAt(out.terrain, wx, wz);
            float const trunkH = 2.4f + u01(rng) * 3.8f;
            float const trunkR = 0.11f + u01(rng) * 0.09f;
            Vec3 const trunkCenter{wx, ty + trunkH * 0.5f, wz};
            Vec3 const trunkHalf{trunkR, trunkH * 0.5f, trunkR};
            float const yawT = uAngle(rng);
            float const pitchT = (u01(rng) - 0.5f) * 0.12f;
            float const rollT = (u01(rng) - 0.5f) * 0.12f;
            Aabb const trunkBox = worldAabbOfOrientedUnitBox(trunkCenter, trunkHalf, yawT, pitchT, rollT);

            float const canopyR = 1.15f + u01(rng) * 1.35f;
            float const canopyLift = trunkH * 0.52f + u01(rng) * 0.35f * trunkH;
            Vec3 const canopyCenter{wx, ty + canopyLift, wz};
            Vec3 const canopyHalf{canopyR * 0.55f, canopyR * 0.42f, canopyR * 0.55f};
            float const yawC = uAngle(rng);
            float const pitchC = (u01(rng) - 0.5f) * 0.35f;
            float const rollC = (u01(rng) - 0.5f) * 0.35f;
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
            push(trunkBox, 0.62f, GardenColliderKind::TreeTrunk, GardenPropMesh::TrunkY, yawT, pitchT, rollT);
            push(canopyBox, 0.78f, GardenColliderKind::TreeFoliage, GardenPropMesh::Foliage, yawC, pitchC, rollC);
            break;
        }
    }

    int constexpr kUnderstory = 48;
    for (int u = 0; u < kUnderstory; ++u) {
        for (int tries = 0; tries < 200; ++tries) {
            float const ang = uAngle(rng);
            float const rad = 10.f + u01(rng) * (R - 18.f);
            float const wx = std::cos(ang) * rad;
            float const wz = std::sin(ang) * rad;
            if (gardenTerrainSlopeMagnitude(out.terrain, wx, wz) > 0.5f) {
                continue;
            }
            float const ty = gardenTerrainHeightAt(out.terrain, wx, wz);
            float const hr = 0.08f + u01(rng) * 0.14f;
            Vec3 half{hr, hr * 0.35f, hr};
            Vec3 c{wx, ty + half.y + 0.002f, wz};
            float const yawL = uAngle(rng);
            float const pitchL = (u01(rng) - 0.5f) * 0.5f;
            float const rollL = (u01(rng) - 0.5f) * 0.5f;
            Aabb const candidate = worldAabbOfOrientedUnitBox(c, half, yawL, pitchL, rollL);
            if (overlapsAny(candidate, kPropPad)) {
                continue;
            }
            push(candidate, 0.85f, GardenColliderKind::Leaf, GardenPropMesh::Icosahedron, yawL, pitchL, rollL);
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

    std::vector<math::Vec3> vel(debris.size(), math::Vec3::zero());
    math::Vec3 const g{0.f, -9.81f, 0.f};

    for (int step = 0; step < kMaxSteps; ++step) {
        for (std::size_t k = 0; k < debris.size(); ++k) {
            std::size_t const idx = debris[k];
            vel[k] = vel[k] + g * dt;
            math::Vec3 const h =
                (layout.staticColliders[idx].max - layout.staticColliders[idx].min) * 0.5f;
            math::Vec3 c =
                (layout.staticColliders[idx].min + layout.staticColliders[idx].max) * 0.5f + vel[k] * dt;
            writeAabbFromCenterHalf(c, h, layout.staticColliders[idx]);
        }

        for (int it = 0; it < kInnerIters; ++it) {
            for (std::size_t k = 0; k < debris.size(); ++k) {
                std::size_t const idx = debris[k];
                math::Aabb& box = layout.staticColliders[idx];
                math::Vec3 c = (box.min + box.max) * 0.5f;
                math::Vec3 const h = (box.max - box.min) * 0.5f;
                for (std::size_t j = 0; j < n; ++j) {
                    if (j == idx || isLogBakeKind(layout.kinds[j])) {
                        continue;
                    }
                    (void)separateDebrisFromStatic(c, h, vel[k], layout.staticColliders[j], kStaticTangent, kRest);
                }
                writeAabbFromCenterHalf(c, h, box);
            }

            for (std::size_t a = 0; a < debris.size(); ++a) {
                for (std::size_t b = a + 1; b < debris.size(); ++b) {
                    std::size_t const ia = debris[a];
                    std::size_t const ib = debris[b];
                    math::Aabb& ba = layout.staticColliders[ia];
                    math::Aabb& bb = layout.staticColliders[ib];
                    math::Vec3 ca = (ba.min + ba.max) * 0.5f;
                    math::Vec3 ha = (ba.max - ba.min) * 0.5f;
                    math::Vec3 cb = (bb.min + bb.max) * 0.5f;
                    math::Vec3 hb = (bb.max - bb.min) * 0.5f;
                    float const ra = std::max(ha.x, std::max(ha.y, ha.z));
                    float const rb = std::max(hb.x, std::max(hb.y, hb.z));
                    math::Vec3 const d = ca - cb;
                    float const reach = ra + rb + 0.55f;
                    if (math::lengthSquared(d) > reach * reach) {
                        continue;
                    }
                    float const invA = gardenLooseBodyInverseMass(layout.kinds[ia]);
                    float const invB = gardenLooseBodyInverseMass(layout.kinds[ib]);
                    separateDebrisPairWeighted(ca, ha, vel[a], invA, cb, hb, vel[b], invB, kRest, kPairTangent);
                    writeAabbFromCenterHalf(ca, ha, ba);
                    writeAabbFromCenterHalf(cb, hb, bb);
                }
            }
        }

        for (std::size_t k = 0; k < debris.size(); ++k) {
            std::size_t const idx = debris[k];
            math::Aabb& box = layout.staticColliders[idx];
            math::Vec3 c = (box.min + box.max) * 0.5f;
            math::Vec3 const h = (box.max - box.min) * 0.5f;
            float const ground = gardenTerrainHeightAt(layout.terrain, c.x, c.z);
            if (c.y - h.y < ground) {
                c.y = ground + h.y;
                if (vel[k].y < 0.f) {
                    vel[k].y = 0.f;
                }
                writeAabbFromCenterHalf(c, h, box);
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
}

void placeMarblesInArena(std::array<physics::RigidBodyKinematics, 2>& marbles, GardenLayout const& layout) noexcept {
    marbles[0] = {};
    marbles[1] = {};
    float const invMass = 1.f / kPlayerBallMassKg;
    marbles[0].invMass = invMass;
    marbles[1].invMass = invMass;
    float const halfSep = kArenaRadius * 0.35f;
    // Extra clearance above sampled terrain: static props sit on the heightfield but are not reflected in
    // gardenTerrainHeightAt(); Jolt's heightfield can also sit slightly above the analytic samples after
    // quantization. Without this, spheres can spawn embedded and appear stuck (no move / no jump).
    constexpr float kSpawnExtraClearance = 0.12f;
    float const y0 =
        gardenTerrainHeightAt(layout.terrain, -halfSep, 0.f) + kMarbleRadius + kSpawnExtraClearance;
    float const y1 =
        gardenTerrainHeightAt(layout.terrain, halfSep, 0.f) + kMarbleRadius + kSpawnExtraClearance;
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
    float const ty = gardenTerrainHeightAt(layout.terrain, ball.position.x, ball.position.z);
    float const bottom = ball.position.y - radius;
    if (ball.linearVelocity.y > 0.9f) {
        return false;
    }
    return bottom <= ty + 0.36f;
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

} // namespace marble::garden
