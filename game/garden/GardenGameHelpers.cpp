#include "garden/GardenGameHelpers.hpp"

#include "physics/PhysicsIntegration.hpp"
#include "render/ProceduralMeshVulkan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace marble::garden_app::detail {

void fillTerrainMeshCpu(
    marble::garden::GardenTerrain const& t,
    std::vector<marble::render::VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx
) {
    vtx.clear();
    idx.clear();
    std::uint32_t const N = t.sampleCount;
    if (N < 2u || t.heights.size() != static_cast<std::size_t>(N) * static_cast<std::size_t>(N)) {
        return;
    }
    float const cell = t.cellSize;
    float const hSpan = std::max(t.maxHeight - t.minHeight, 0.05f);
    auto heightAt = [&](int ix, int iz) -> float {
        ix = std::clamp(ix, 0, static_cast<int>(N) - 1);
        iz = std::clamp(iz, 0, static_cast<int>(N) - 1);
        return t.heights[static_cast<std::size_t>(iz) * static_cast<std::size_t>(N) + static_cast<std::size_t>(ix)];
    };
    for (std::uint32_t iz = 0; iz < N; ++iz) {
        for (std::uint32_t ix = 0; ix < N; ++ix) {
            float const wx = t.origin.x + static_cast<float>(ix) * cell;
            float const wz = t.origin.z + static_cast<float>(iz) * cell;
            float const y = t.heights[static_cast<std::size_t>(iz) * static_cast<std::size_t>(N) + ix];
            int const ixi = static_cast<int>(ix);
            int const izi = static_cast<int>(iz);
            float const dhx = (heightAt(ixi + 1, izi) - heightAt(ixi - 1, izi)) / (2.f * cell);
            float const dhz = (heightAt(ixi, izi + 1) - heightAt(ixi, izi - 1)) / (2.f * cell);
            marble::math::Vec3 n{-dhx, 1.f, -dhz};
            float const nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (nl > 1e-6f) {
                n = n * (1.f / nl);
            } else {
                n = marble::math::Vec3::unitY();
            }
            float const hn = (y - t.minHeight) / hSpan;
            marble::math::Vec3 const col{
                0.14f + hn * 0.12f,
                0.28f + hn * 0.18f + std::fabs(dhx + dhz) * 0.04f,
                0.10f + hn * 0.07f,
            };
            marble::render::pushVert(vtx, {wx, y, wz}, n, col);
        }
    }
    for (std::uint32_t iz = 0; iz < N - 1u; ++iz) {
        for (std::uint32_t ix = 0; ix < N - 1u; ++ix) {
            std::uint32_t const v00 = iz * N + ix;
            std::uint32_t const v10 = iz * N + ix + 1u;
            std::uint32_t const v01 = (iz + 1u) * N + ix;
            std::uint32_t const v11 = (iz + 1u) * N + ix + 1u;
            idx.push_back(v00);
            idx.push_back(v10);
            idx.push_back(v01);
            idx.push_back(v10);
            idx.push_back(v11);
            idx.push_back(v01);
        }
    }
}

marble::math::Vec3 colorForWallBrick(marble::math::Aabb const& b) noexcept {
    float const ymin = b.min.y;
    if (ymin < 0.085f) {
        return {0.22f, 0.30f, 0.20f};
    }
    if (ymin < 0.19f) {
        return {0.28f, 0.36f, 0.24f};
    }
    if (ymin < 0.31f) {
        return {0.32f, 0.40f, 0.26f};
    }
    return {0.30f, 0.38f, 0.25f};
}

marble::math::Vec3 colorForKind(marble::garden::GardenColliderKind k, std::uint32_t salt) noexcept {
    using marble::math::Vec3;
    auto hashTint = [](std::uint32_t s, Vec3 base, float spread) -> Vec3 {
        float const t = static_cast<float>(s % 997u) * (1.f / 997.f);
        float const u = static_cast<float>((s / 997u) % 997u) * (1.f / 997.f);
        return {
            std::clamp(base.x + (t - 0.5f) * spread, 0.f, 1.f),
            std::clamp(base.y + (u - 0.5f) * spread, 0.f, 1.f),
            std::clamp(base.z + ((t + u) * 0.5f - 0.5f) * spread, 0.f, 1.f),
        };
    };
    switch (k) {
    case marble::garden::GardenColliderKind::Ground:
        return hashTint(salt, {0.22f, 0.38f, 0.16f}, 0.08f);
    case marble::garden::GardenColliderKind::Terrace:
        return hashTint(salt, {0.26f, 0.42f, 0.18f}, 0.09f);
    case marble::garden::GardenColliderKind::Wall:
        return hashTint(salt, {0.28f, 0.32f, 0.24f}, 0.07f);
    case marble::garden::GardenColliderKind::GrassBump:
        return hashTint(salt, {0.20f, 0.42f, 0.18f}, 0.1f);
    case marble::garden::GardenColliderKind::SandPit:
        return hashTint(salt, {0.32f, 0.28f, 0.18f}, 0.1f);
    case marble::garden::GardenColliderKind::Boulder:
        return hashTint(salt, {0.30f, 0.32f, 0.30f}, 0.06f);
    case marble::garden::GardenColliderKind::Rock:
        return hashTint(salt, {0.42f, 0.44f, 0.40f}, 0.055f);
    case marble::garden::GardenColliderKind::Leaf:
        return hashTint(salt, {0.14f, 0.42f, 0.16f}, 0.08f);
    case marble::garden::GardenColliderKind::Twig:
        return hashTint(salt, {0.24f, 0.16f, 0.10f}, 0.1f);
    case marble::garden::GardenColliderKind::Log:
        return hashTint(salt, {0.32f, 0.22f, 0.14f}, 0.07f);
    case marble::garden::GardenColliderKind::TwigScatter:
        return hashTint(salt, {0.22f, 0.15f, 0.10f}, 0.08f);
    case marble::garden::GardenColliderKind::TreeTrunk:
        return hashTint(salt, {0.22f, 0.14f, 0.08f}, 0.06f);
    case marble::garden::GardenColliderKind::TreeFoliage:
        return hashTint(salt, {0.10f, 0.36f, 0.12f}, 0.1f);
    case marble::garden::GardenColliderKind::HeroPlayground:
        return hashTint(salt, {0.55f, 0.38f, 0.62f}, 0.06f);
    }
    return {0.5f, 0.5f, 0.5f};
}

void worldRayFromWindowPixel(
    float mouseX,
    float mouseY,
    int fbW,
    int fbH,
    marble::math::Vec3 eye,
    marble::math::Vec3 target,
    float fovyRad,
    float aspect,
    marble::math::Vec3& outOrigin,
    marble::math::Vec3& outDir
) noexcept {
    outOrigin = eye;
    if (fbW < 1 || fbH < 1) {
        outDir = marble::math::normalize(target - eye);
        return;
    }
    float const ndcX = (2.f * mouseX / static_cast<float>(fbW)) - 1.f;
    float const ndcY = 1.f - (2.f * mouseY / static_cast<float>(fbH));
    marble::math::Vec3 const f = marble::math::normalize(target - eye);
    marble::math::Vec3 worldUp = marble::math::Vec3::unitY();
    marble::math::Vec3 r = marble::math::normalize(marble::math::cross(worldUp, f));
    if (marble::math::lengthSquared(r) < 1e-8f) {
        r = marble::math::Vec3::unitX();
    }
    marble::math::Vec3 const u = marble::math::cross(f, r);
    float const tanHalf = std::tan(fovyRad * 0.5f);
    marble::math::Vec3 dir = f + r * (ndcX * tanHalf * aspect) + u * (ndcY * tanHalf);
    outDir = marble::math::normalize(dir);
}

void applyLawnStrokeImpulse(
    marble::physics::RigidBodyKinematics& marble,
    marble::math::Vec3 const& pStrokeStart,
    marble::math::Vec3 const& pStrokeLast,
    marble::math::Vec3 const& pStrokePrev,
    float flickVelEmaX,
    float flickVelEmaZ,
    bool flickEmaValid,
    float deltaSeconds
) noexcept {
    marble::math::Vec3 const d = pStrokeLast - pStrokeStart;
    float const dx = d.x;
    float const dz = d.z;
    float const strokeLen = std::sqrt(dx * dx + dz * dz);

    float const invDt = 1.f / std::max(deltaSeconds, 1e-4f);
    float const instVx = (pStrokeLast.x - pStrokePrev.x) * invDt;
    float const instVz = (pStrokeLast.z - pStrokePrev.z) * invDt;
    float vx = flickEmaValid ? flickVelEmaX : instVx;
    float vz = flickEmaValid ? flickVelEmaZ : instVz;
    float const flickLen = std::sqrt(vx * vx + vz * vz);

    constexpr float kMinStroke = 0.03f;
    constexpr float kMinFlick = 0.28f;
    if (strokeLen < kMinStroke && flickLen < kMinFlick) {
        return;
    }

    marble::math::Vec3 dir{};
    if (strokeLen >= kMinStroke) {
        float const inv = 1.f / strokeLen;
        dir = {dx * inv, 0.f, dz * inv};
    } else {
        float const inv = 1.f / flickLen;
        dir = {vx * inv, 0.f, vz * inv};
    }

    constexpr float kDrag = 0.98f;
    constexpr float kFlick = 0.19f;
    constexpr float kMaxImpulse = 14.5f;
    constexpr float kPower = 0.78f;
    float const rawMag = kDrag * strokeLen + kFlick * flickLen;
    float const curved = std::pow(std::max(rawMag, 0.f), kPower);
    float const mag = std::min(kMaxImpulse, curved);
    marble::physics::applyImpulseLinear(marble, marble::math::Vec3{dir.x * mag, 0.f, dir.z * mag});
}

} // namespace marble::garden_app::detail
