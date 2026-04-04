#include "GardenGame.hpp"

#include "core/ResourceManager.hpp"
#include "core/Simulation.hpp"
#include "garden/GardenSimulation.hpp"
#include "shared/PauseMenuInput.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "input/PlatformGamepadBridge.hpp"
#include "input/PlatformKeyboardBridge.hpp"
#include "math/Geometry.hpp"
#include "math/Mat4.hpp"
#include "math/Vec3.hpp"
#include "physics/PhysicsIntegration.hpp"
#include "physics/RigidBodyDynamics.hpp"
#include "platform/window/Window.hpp"
#include "render/IRenderBackend.hpp"
#include "render/RenderTypes.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace marble::garden_app {

namespace {

using marble::garden::GardenColliderKind;
using marble::garden::GardenLayout;
using marble::garden::GardenPropMesh;
using marble::garden::GardenTerrain;
using marble::garden::buildGardenLayout;
using marble::garden::kGardenRadius;
using marble::garden::kMarbleRadius;
using marble::garden::placeMarblesInArena;
using marble::garden::rayHitsSphere;
using marble::garden::rayIntersectHorizontalPlane;
using marble::garden::gardenTerrainHeightAt;
using marble::input::AbstractControl;
using marble::input::AbstractControlArray;
using marble::input::ActionContextEntry;
using marble::input::ActionPolicy;
using marble::input::ControlValueClass;
using marble::input::InputRemapTable;
using marble::input::LogicalActionId;
using marble::input::LogicalDevice;
using marble::input::actionScalar;
using marble::input::deviceMask;
using marble::input::anyStandardGamepadPresent;
using marble::input::mergeAllConnectedGamepadsIntoAbstractControls;
using marble::input::sampleKeyboardIntoAbstractControls;
using marble::math::Aabb;
using marble::math::Mat4;
using marble::math::Vec3;
using marble::physics::PhysicsBodyMaterial;
using marble::physics::PhysicsStaticHeightFieldDesc;
using marble::physics::PhysicsCylindricalXZClamp;
using marble::physics::PhysicsDynamicSphereDesc;
using marble::physics::PhysicsStaticBoxDesc;
using marble::physics::PhysicsStepOptions;
using marble::physics::RigidBodyKinematics;
using marble::physics::IPhysicsWorld;
using marble::physics::SimplePhysicsWorld;
using marble::physics::createJoltPhysicsScene;
using marble::physics::applyImpulseLinear;
using marble::platform::Key;
using marble::platform::MouseButton;
using marble::render::FrameOverlayTint;
using marble::render::IRenderBackend;
using marble::render::MeshDrawInstance;
using marble::render::VulkanRhi;

/// Esc / Back toggles pause. While paused, Q returns to the app menu (not during live play).
static constexpr LogicalActionId kActionPauseMenu = 1;

static constexpr ActionContextEntry kGameplayContext[] = {
    {kActionPauseMenu, deviceMask(LogicalDevice::Player), false},
};

enum class PausePanel : std::uint8_t {
    Main,
    Options,
};

[[nodiscard]] Mat4 lookAtLh(Vec3 const& eye, Vec3 const& target, Vec3 const& worldUp) noexcept {
    Vec3 const f = marble::math::normalize(target - eye);
    Vec3 mutUp = worldUp;
    Vec3 r = marble::math::cross(mutUp, f);
    if (marble::math::lengthSquared(r) < 1e-8f) {
        mutUp = Vec3::unitX();
        r = marble::math::cross(mutUp, f);
    }
    r = marble::math::normalize(r);
    Vec3 const u = marble::math::cross(f, r);
    Mat4 m{};
    m.m[0] = r.x;
    m.m[1] = u.x;
    m.m[2] = -f.x;
    m.m[3] = 0.f;
    m.m[4] = r.y;
    m.m[5] = u.y;
    m.m[6] = -f.y;
    m.m[7] = 0.f;
    m.m[8] = r.z;
    m.m[9] = u.z;
    m.m[10] = -f.z;
    m.m[11] = 0.f;
    m.m[12] = -marble::math::dot(r, eye);
    m.m[13] = -marble::math::dot(u, eye);
    m.m[14] = marble::math::dot(f, eye);
    m.m[15] = 1.f;
    return m;
}

[[nodiscard]] Mat4 perspectiveVulkan(float fovyRad, float aspect, float n, float f) noexcept {
    float const t = std::tan(fovyRad * 0.5f);
    if (t <= 1e-8f) {
        return Mat4::identity();
    }
    float const invT = 1.f / t;
    Mat4 p{};
    p.m[0] = invT / aspect;
    p.m[5] = invT;
    p.m[10] = f / (n - f);
    p.m[11] = -1.f;
    p.m[14] = (n * f) / (n - f);
    return p;
}

void addCube(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, Vec3 color) {
    auto face = [&](Vec3 n, Vec3 t0, Vec3 t1, Vec3 t2, Vec3 t3) {
        std::uint32_t base = static_cast<std::uint32_t>(vtx.size());
        auto push = [&](Vec3 p) {
            VulkanRhi::Vertex v{};
            v.px = p.x;
            v.py = p.y;
            v.pz = p.z;
            v.nx = n.x;
            v.ny = n.y;
            v.nz = n.z;
            v.cr = color.x;
            v.cg = color.y;
            v.cb = color.z;
            vtx.push_back(v);
        };
        push(t0);
        push(t1);
        push(t2);
        push(t3);
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
        idx.push_back(base + 2);
        idx.push_back(base + 3);
        idx.push_back(base);
    };
    float const h = 0.5f;
    face({1.f, 0.f, 0.f}, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h});
    face({-1.f, 0.f, 0.f}, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h});
    face({0.f, 1.f, 0.f}, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h});
    face({0.f, -1.f, 0.f}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});
    face({0.f, 0.f, 1.f}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});
    face({0.f, 0.f, -1.f}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});
}

void addUvSphere(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    float radius,
    int stacks,
    int slices,
    Vec3 color
) {
    if (stacks < 2 || slices < 3) {
        return;
    }
    std::uint32_t base = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i <= stacks; ++i) {
        float const v = static_cast<float>(i) / static_cast<float>(stacks);
        float const phi = v * 3.14159265f;
        float const sp = std::sin(phi);
        float const cp = std::cos(phi);
        for (int j = 0; j <= slices; ++j) {
            float const u = static_cast<float>(j) / static_cast<float>(slices);
            float const theta = u * 2.f * 3.14159265f;
            float const st = std::sin(theta);
            float const ct = std::cos(theta);
            float const nx = ct * sp;
            float const ny = cp;
            float const nz = st * sp;
            VulkanRhi::Vertex vert{};
            vert.px = nx * radius;
            vert.py = ny * radius;
            vert.pz = nz * radius;
            vert.nx = nx;
            vert.ny = ny;
            vert.nz = nz;
            vert.cr = color.x;
            vert.cg = color.y;
            vert.cb = color.z;
            vtx.push_back(vert);
        }
    }
    int const stride = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int const a = base + i * stride + j;
            int const b = base + (i + 1) * stride + j;
            idx.push_back(static_cast<std::uint32_t>(a));
            idx.push_back(static_cast<std::uint32_t>(b));
            idx.push_back(static_cast<std::uint32_t>(a + 1));
            idx.push_back(static_cast<std::uint32_t>(a + 1));
            idx.push_back(static_cast<std::uint32_t>(b));
            idx.push_back(static_cast<std::uint32_t>(b + 1));
        }
    }
}

void pushVert(std::vector<VulkanRhi::Vertex>& vtx, Vec3 p, Vec3 n, Vec3 color) {
    VulkanRhi::Vertex v{};
    v.px = p.x;
    v.py = p.y;
    v.pz = p.z;
    v.nx = n.x;
    v.ny = n.y;
    v.nz = n.z;
    v.cr = color.x;
    v.cg = color.y;
    v.cb = color.z;
    vtx.push_back(v);
}

void addOctahedron(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, Vec3 color) {
    float const h = 0.5f;
    std::uint32_t const o = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {h, 0.f, 0.f}, {1.f, 0.f, 0.f}, color);
    pushVert(vtx, {-h, 0.f, 0.f}, {-1.f, 0.f, 0.f}, color);
    pushVert(vtx, {0.f, h, 0.f}, {0.f, 1.f, 0.f}, color);
    pushVert(vtx, {0.f, -h, 0.f}, {0.f, -1.f, 0.f}, color);
    pushVert(vtx, {0.f, 0.f, h}, {0.f, 0.f, 1.f}, color);
    pushVert(vtx, {0.f, 0.f, -h}, {0.f, 0.f, -1.f}, color);
    int const faces[8][3] = {
        {0, 2, 4},
        {0, 4, 3},
        {0, 3, 5},
        {0, 5, 2},
        {1, 4, 2},
        {1, 3, 4},
        {1, 5, 3},
        {1, 2, 5},
    };
    for (auto const& f : faces) {
        Vec3 const a{vtx[o + static_cast<std::uint32_t>(f[0])].px, vtx[o + static_cast<std::uint32_t>(f[0])].py,
            vtx[o + static_cast<std::uint32_t>(f[0])].pz};
        Vec3 const b{vtx[o + static_cast<std::uint32_t>(f[1])].px, vtx[o + static_cast<std::uint32_t>(f[1])].py,
            vtx[o + static_cast<std::uint32_t>(f[1])].pz};
        Vec3 const c3{vtx[o + static_cast<std::uint32_t>(f[2])].px, vtx[o + static_cast<std::uint32_t>(f[2])].py,
            vtx[o + static_cast<std::uint32_t>(f[2])].pz};
        Vec3 e0 = b - a;
        Vec3 e1 = c3 - a;
        Vec3 n = marble::math::cross(e0, e1);
        float const ln = std::sqrt(marble::math::lengthSquared(n));
        if (ln > 1e-8f) {
            n = n * (1.f / ln);
        } else {
            n = {0.f, 1.f, 0.f};
        }
        for (int k = 0; k < 3; ++k) {
            vtx[o + static_cast<std::uint32_t>(f[k])].nx = n.x;
            vtx[o + static_cast<std::uint32_t>(f[k])].ny = n.y;
            vtx[o + static_cast<std::uint32_t>(f[k])].nz = n.z;
        }
        idx.push_back(o + static_cast<std::uint32_t>(f[0]));
        idx.push_back(o + static_cast<std::uint32_t>(f[1]));
        idx.push_back(o + static_cast<std::uint32_t>(f[2]));
    }
}

void addIcosahedron(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, Vec3 color) {
    float const phi = (1.f + std::sqrt(5.f)) * 0.5f;
    Vec3 raw[12] = {
        {0.f, 1.f, phi},
        {0.f, 1.f, -phi},
        {0.f, -1.f, phi},
        {0.f, -1.f, -phi},
        {1.f, phi, 0.f},
        {1.f, -phi, 0.f},
        {-1.f, phi, 0.f},
        {-1.f, -phi, 0.f},
        {phi, 0.f, 1.f},
        {phi, 0.f, -1.f},
        {-phi, 0.f, 1.f},
        {-phi, 0.f, -1.f},
    };
    std::uint32_t const base = static_cast<std::uint32_t>(vtx.size());
    for (Vec3 p : raw) {
        float const len = std::sqrt(marble::math::lengthSquared(p));
        float const s = (len > 1e-8f) ? (0.5f / len) : 0.f;
        p = p * s;
        Vec3 const n = (s > 1e-12f) ? p * (1.f / 0.5f) : Vec3{0.f, 1.f, 0.f};
        pushVert(vtx, p, n, color);
    }
    int const faces[20][3] = {
        {0, 4, 8},
        {0, 8, 10},
        {0, 10, 6},
        {0, 6, 1},
        {0, 1, 4},
        {4, 1, 9},
        {4, 9, 5},
        {4, 5, 8},
        {8, 5, 2},
        {8, 2, 10},
        {10, 2, 7},
        {10, 7, 6},
        {6, 7, 3},
        {6, 3, 1},
        {1, 3, 9},
        {3, 7, 5},
        {3, 5, 2},
        {3, 2, 10},
        {7, 2, 5},
        {7, 5, 9},
    };
    for (auto const& f : faces) {
        std::uint32_t const ia = base + static_cast<std::uint32_t>(f[0]);
        std::uint32_t const ib = base + static_cast<std::uint32_t>(f[1]);
        std::uint32_t const ic = base + static_cast<std::uint32_t>(f[2]);
        Vec3 const a{vtx[ia].px, vtx[ia].py, vtx[ia].pz};
        Vec3 const b{vtx[ib].px, vtx[ib].py, vtx[ib].pz};
        Vec3 const c{vtx[ic].px, vtx[ic].py, vtx[ic].pz};
        Vec3 e0 = b - a;
        Vec3 e1 = c - a;
        Vec3 n = marble::math::cross(e0, e1);
        float const ln = std::sqrt(marble::math::lengthSquared(n));
        if (ln > 1e-8f) {
            n = n * (1.f / ln);
        } else {
            n = {0.f, 1.f, 0.f};
        }
        vtx[ia].nx = n.x;
        vtx[ia].ny = n.y;
        vtx[ia].nz = n.z;
        vtx[ib].nx = n.x;
        vtx[ib].ny = n.y;
        vtx[ib].nz = n.z;
        vtx[ic].nx = n.x;
        vtx[ic].ny = n.y;
        vtx[ic].nz = n.z;
        idx.push_back(ia);
        idx.push_back(ib);
        idx.push_back(ic);
    }
}

/// One level of midpoint subdivision on the unit icosa (≈80 faces), projected back onto the sphere — no noise.
void addIcosahedronSubdividedClean(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    Vec3 color
) {
    float const phi = (1.f + std::sqrt(5.f)) * 0.5f;
    std::vector<Vec3> p = {
        {0.f, 1.f, phi},
        {0.f, 1.f, -phi},
        {0.f, -1.f, phi},
        {0.f, -1.f, -phi},
        {1.f, phi, 0.f},
        {1.f, -phi, 0.f},
        {-1.f, phi, 0.f},
        {-1.f, -phi, 0.f},
        {phi, 0.f, 1.f},
        {phi, 0.f, -1.f},
        {-phi, 0.f, 1.f},
        {-phi, 0.f, -1.f},
    };
    for (Vec3& v : p) {
        float const len = std::sqrt(marble::math::lengthSquared(v));
        if (len > 1e-8f) {
            v = v * (0.5f / len);
        }
    }

    static constexpr int faces[20][3] = {
        {0, 4, 8},
        {0, 8, 10},
        {0, 10, 6},
        {0, 6, 1},
        {0, 1, 4},
        {4, 1, 9},
        {4, 9, 5},
        {4, 5, 8},
        {8, 5, 2},
        {8, 2, 10},
        {10, 2, 7},
        {10, 7, 6},
        {6, 7, 3},
        {6, 3, 1},
        {1, 3, 9},
        {3, 7, 5},
        {3, 5, 2},
        {3, 2, 10},
        {7, 2, 5},
        {7, 5, 9},
    };

    std::vector<std::array<std::uint32_t, 3>> tris;
    tris.reserve(20);
    for (auto const& f : faces) {
        tris.push_back(
            {static_cast<std::uint32_t>(f[0]), static_cast<std::uint32_t>(f[1]), static_cast<std::uint32_t>(f[2])}
        );
    }

    std::unordered_map<std::uint64_t, std::uint32_t> edgeMid;
    auto getMidpoint = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
        std::uint32_t const lo = a < b ? a : b;
        std::uint32_t const hi = a < b ? b : a;
        std::uint64_t const key = (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
        auto const it = edgeMid.find(key);
        if (it != edgeMid.end()) {
            return it->second;
        }
        Vec3 const pa = p[a];
        Vec3 const pb = p[b];
        Vec3 m = (pa + pb) * 0.5f;
        float const lm = std::sqrt(marble::math::lengthSquared(m));
        if (lm > 1e-8f) {
            m = m * (0.5f / lm);
        } else {
            m = Vec3{0.f, 0.5f, 0.f};
        }
        std::uint32_t const ni = static_cast<std::uint32_t>(p.size());
        p.push_back(m);
        edgeMid[key] = ni;
        return ni;
    };

    {
        std::vector<std::array<std::uint32_t, 3>> next;
        next.reserve(tris.size() * 4);
        for (auto const& t : tris) {
            std::uint32_t const a = t[0];
            std::uint32_t const b = t[1];
            std::uint32_t const c = t[2];
            std::uint32_t const ab = getMidpoint(a, b);
            std::uint32_t const bc = getMidpoint(b, c);
            std::uint32_t const ca = getMidpoint(c, a);
            next.push_back({a, ab, ca});
            next.push_back({b, bc, ab});
            next.push_back({c, ca, bc});
            next.push_back({ab, bc, ca});
        }
        tris = std::move(next);
    }

    for (auto const& t : tris) {
        Vec3 const va = p[t[0]];
        Vec3 const vb = p[t[1]];
        Vec3 const vc = p[t[2]];
        Vec3 e0 = vb - va;
        Vec3 e1 = vc - va;
        Vec3 fn = marble::math::cross(e0, e1);
        float const ln = std::sqrt(marble::math::lengthSquared(fn));
        if (ln > 1e-8f) {
            fn = fn * (1.f / ln);
        } else {
            fn = Vec3{0.f, 1.f, 0.f};
        }
        std::uint32_t const base = static_cast<std::uint32_t>(vtx.size());
        pushVert(vtx, va, fn, color);
        pushVert(vtx, vb, fn, color);
        pushVert(vtx, vc, fn, color);
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
    }
}

void addIcosahedronSubdividedNoisy(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    Vec3 color,
    std::uint32_t salt
) {
    float const phi = (1.f + std::sqrt(5.f)) * 0.5f;
    std::vector<Vec3> p = {
        {0.f, 1.f, phi},
        {0.f, 1.f, -phi},
        {0.f, -1.f, phi},
        {0.f, -1.f, -phi},
        {1.f, phi, 0.f},
        {1.f, -phi, 0.f},
        {-1.f, phi, 0.f},
        {-1.f, -phi, 0.f},
        {phi, 0.f, 1.f},
        {phi, 0.f, -1.f},
        {-phi, 0.f, 1.f},
        {-phi, 0.f, -1.f},
    };
    for (Vec3& v : p) {
        float const len = std::sqrt(marble::math::lengthSquared(v));
        if (len > 1e-8f) {
            v = v * (0.5f / len);
        }
    }

    static constexpr int faces[20][3] = {
        {0, 4, 8},
        {0, 8, 10},
        {0, 10, 6},
        {0, 6, 1},
        {0, 1, 4},
        {4, 1, 9},
        {4, 9, 5},
        {4, 5, 8},
        {8, 5, 2},
        {8, 2, 10},
        {10, 2, 7},
        {10, 7, 6},
        {6, 7, 3},
        {6, 3, 1},
        {1, 3, 9},
        {3, 7, 5},
        {3, 5, 2},
        {3, 2, 10},
        {7, 2, 5},
        {7, 5, 9},
    };

    std::vector<std::array<std::uint32_t, 3>> tris;
    tris.reserve(20);
    for (auto const& f : faces) {
        tris.push_back(
            {static_cast<std::uint32_t>(f[0]), static_cast<std::uint32_t>(f[1]), static_cast<std::uint32_t>(f[2])}
        );
    }

    std::unordered_map<std::uint64_t, std::uint32_t> edgeMid;
    auto getMidpoint = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
        std::uint32_t const lo = a < b ? a : b;
        std::uint32_t const hi = a < b ? b : a;
        std::uint64_t const key = (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
        auto const it = edgeMid.find(key);
        if (it != edgeMid.end()) {
            return it->second;
        }
        Vec3 const pa = p[a];
        Vec3 const pb = p[b];
        Vec3 m = (pa + pb) * 0.5f;
        float const lm = std::sqrt(marble::math::lengthSquared(m));
        if (lm > 1e-8f) {
            m = m * (0.5f / lm);
        } else {
            m = Vec3{0.f, 0.5f, 0.f};
        }
        std::uint32_t const ni = static_cast<std::uint32_t>(p.size());
        p.push_back(m);
        edgeMid[key] = ni;
        return ni;
    };

    {
        std::vector<std::array<std::uint32_t, 3>> next;
        next.reserve(tris.size() * 4);
        for (auto const& t : tris) {
            std::uint32_t const a = t[0];
            std::uint32_t const b = t[1];
            std::uint32_t const c = t[2];
            std::uint32_t const ab = getMidpoint(a, b);
            std::uint32_t const bc = getMidpoint(b, c);
            std::uint32_t const ca = getMidpoint(c, a);
            next.push_back({a, ab, ca});
            next.push_back({b, bc, ab});
            next.push_back({c, ca, bc});
            next.push_back({ab, bc, ca});
        }
        tris = std::move(next);
    }

    for (std::size_t i = 0; i < p.size(); ++i) {
        Vec3 const nrm = marble::math::normalize(p[i]);
        std::uint32_t const h = salt * 1664525u + static_cast<std::uint32_t>(i) * 1013904223u;
        float const u = static_cast<float>(h & 0xffffu) * (1.f / 65535.f);
        float const disp = (u - 0.5f) * 0.09f;
        p[i] = nrm * (0.5f + disp);
    }

    for (auto const& t : tris) {
        Vec3 const va = p[t[0]];
        Vec3 const vb = p[t[1]];
        Vec3 const vc = p[t[2]];
        Vec3 e0 = vb - va;
        Vec3 e1 = vc - va;
        Vec3 fn = marble::math::cross(e0, e1);
        float const ln = std::sqrt(marble::math::lengthSquared(fn));
        if (ln > 1e-8f) {
            fn = fn * (1.f / ln);
        } else {
            fn = Vec3{0.f, 1.f, 0.f};
        }
        std::uint32_t const base = static_cast<std::uint32_t>(vtx.size());
        pushVert(vtx, va, fn, color);
        pushVert(vtx, vb, fn, color);
        pushVert(vtx, vc, fn, color);
        idx.push_back(base);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
    }
}

void addDiscXZ(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, int segments, Vec3 color) {
    if (segments < 3) {
        return;
    }
    std::uint32_t const c0 = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, color);
    for (int i = 0; i <= segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const x = std::cos(ang) * 0.5f;
        float const z = std::sin(ang) * 0.5f;
        pushVert(vtx, {x, 0.f, z}, {0.f, 1.f, 0.f}, color);
    }
    for (int i = 0; i < segments; ++i) {
        idx.push_back(c0);
        idx.push_back(c0 + 1 + static_cast<std::uint32_t>(i));
        idx.push_back(c0 + 2 + static_cast<std::uint32_t>(i));
    }
}

/// Closed “puck” in XZ (radius 0.5), height 1 along Y from -0.5..0.5 — scales to a solid sand slab, not a single sheet.
void addDiscExtrudedY(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, int segments, Vec3 color) {
    if (segments < 3) {
        return;
    }
    float const y0 = -0.5f;
    float const y1 = 0.5f;
    float const r = 0.5f;

    std::uint32_t const bc = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {0.f, y0, 0.f}, {0.f, -1.f, 0.f}, color);
    for (int i = 0; i <= segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        pushVert(vtx, {std::cos(ang) * r, y0, std::sin(ang) * r}, {0.f, -1.f, 0.f}, color);
    }
    for (int i = 0; i < segments; ++i) {
        idx.push_back(bc);
        idx.push_back(bc + 2 + static_cast<std::uint32_t>(i));
        idx.push_back(bc + 1 + static_cast<std::uint32_t>(i));
    }

    std::uint32_t const tc = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {0.f, y1, 0.f}, {0.f, 1.f, 0.f}, color);
    for (int i = 0; i <= segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        pushVert(vtx, {std::cos(ang) * r, y1, std::sin(ang) * r}, {0.f, 1.f, 0.f}, color);
    }
    for (int i = 0; i < segments; ++i) {
        idx.push_back(tc);
        idx.push_back(tc + 1 + static_cast<std::uint32_t>(i));
        idx.push_back(tc + 2 + static_cast<std::uint32_t>(i));
    }

    std::uint32_t const sideBase = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i <= segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const nx = std::cos(ang);
        float const nz = std::sin(ang);
        Vec3 const n{nx, 0.f, nz};
        pushVert(vtx, {nx * r, y0, nz * r}, n, color);
        pushVert(vtx, {nx * r, y1, nz * r}, n, color);
    }
    for (int i = 0; i < segments; ++i) {
        std::uint32_t const b0 = sideBase + static_cast<std::uint32_t>(i) * 2;
        std::uint32_t const b1 = sideBase + static_cast<std::uint32_t>(i + 1) * 2;
        idx.push_back(b0);
        idx.push_back(b1);
        idx.push_back(b0 + 1);
        idx.push_back(b0 + 1);
        idx.push_back(b1);
        idx.push_back(b1 + 1);
    }
}

/// Solid along **+X**, radius 0.5 in YZ, caps at x = ±0.5 (fits unit cube before non-uniform scale).
void addCylinderAlongX(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, int segments, Vec3 color) {
    if (segments < 3) {
        return;
    }
    float const x0 = -0.5f;
    float const x1 = 0.5f;
    float const r = 0.5f;
    std::uint32_t const ring0 = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const y = std::cos(ang) * r;
        float const z = std::sin(ang) * r;
        Vec3 n{0.f, std::cos(ang), std::sin(ang)};
        pushVert(vtx, {x0, y, z}, n, color);
    }
    std::uint32_t const ring1 = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const y = std::cos(ang) * r;
        float const z = std::sin(ang) * r;
        Vec3 n{0.f, std::cos(ang), std::sin(ang)};
        pushVert(vtx, {x1, y, z}, n, color);
    }
    for (int i = 0; i < segments; ++i) {
        int const j = (i + 1) % segments;
        std::uint32_t const a0 = ring0 + static_cast<std::uint32_t>(i);
        std::uint32_t const a1 = ring0 + static_cast<std::uint32_t>(j);
        std::uint32_t const b0 = ring1 + static_cast<std::uint32_t>(i);
        std::uint32_t const b1 = ring1 + static_cast<std::uint32_t>(j);
        idx.push_back(a0);
        idx.push_back(b0);
        idx.push_back(a1);
        idx.push_back(a1);
        idx.push_back(b0);
        idx.push_back(b1);
    }
    std::uint32_t const capL = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {x0, 0.f, 0.f}, {-1.f, 0.f, 0.f}, color);
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        pushVert(vtx, {x0, std::cos(ang) * r, std::sin(ang) * r}, {-1.f, 0.f, 0.f}, color);
    }
    for (int i = 0; i < segments; ++i) {
        int const j = (i + 1) % segments;
        idx.push_back(capL);
        idx.push_back(capL + 1 + static_cast<std::uint32_t>(j));
        idx.push_back(capL + 1 + static_cast<std::uint32_t>(i));
    }
    std::uint32_t const capR = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {x1, 0.f, 0.f}, {1.f, 0.f, 0.f}, color);
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        pushVert(vtx, {x1, std::cos(ang) * r, std::sin(ang) * r}, {1.f, 0.f, 0.f}, color);
    }
    for (int i = 0; i < segments; ++i) {
        int const j = (i + 1) % segments;
        idx.push_back(capR);
        idx.push_back(capR + 1 + static_cast<std::uint32_t>(i));
        idx.push_back(capR + 1 + static_cast<std::uint32_t>(j));
    }
}

/// Solid along **+Y**, radius 0.5 in XZ, caps at y = ±0.5 (fits unit cube before non-uniform scale).
void addCylinderAlongY(std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx, int segments, Vec3 color) {
    if (segments < 3) {
        return;
    }
    float const y0 = -0.5f;
    float const y1 = 0.5f;
    float const r = 0.5f;
    std::uint32_t const ring0 = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const x = std::cos(ang) * r;
        float const z = std::sin(ang) * r;
        Vec3 const n{std::cos(ang), 0.f, std::sin(ang)};
        pushVert(vtx, {x, y0, z}, n, color);
    }
    std::uint32_t const ring1 = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const x = std::cos(ang) * r;
        float const z = std::sin(ang) * r;
        Vec3 const n{std::cos(ang), 0.f, std::sin(ang)};
        pushVert(vtx, {x, y1, z}, n, color);
    }
    for (int i = 0; i < segments; ++i) {
        int const j = (i + 1) % segments;
        std::uint32_t const a0 = ring0 + static_cast<std::uint32_t>(i);
        std::uint32_t const a1 = ring0 + static_cast<std::uint32_t>(j);
        std::uint32_t const b0 = ring1 + static_cast<std::uint32_t>(i);
        std::uint32_t const b1 = ring1 + static_cast<std::uint32_t>(j);
        idx.push_back(a0);
        idx.push_back(b0);
        idx.push_back(a1);
        idx.push_back(a1);
        idx.push_back(b0);
        idx.push_back(b1);
    }
    std::uint32_t const capB = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {0.f, y0, 0.f}, {0.f, -1.f, 0.f}, color);
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        pushVert(vtx, {std::cos(ang) * r, y0, std::sin(ang) * r}, {0.f, -1.f, 0.f}, color);
    }
    for (int i = 0; i < segments; ++i) {
        int const j = (i + 1) % segments;
        idx.push_back(capB);
        idx.push_back(capB + 1 + static_cast<std::uint32_t>(j));
        idx.push_back(capB + 1 + static_cast<std::uint32_t>(i));
    }
    std::uint32_t const capT = static_cast<std::uint32_t>(vtx.size());
    pushVert(vtx, {0.f, y1, 0.f}, {0.f, 1.f, 0.f}, color);
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        pushVert(vtx, {std::cos(ang) * r, y1, std::sin(ang) * r}, {0.f, 1.f, 0.f}, color);
    }
    for (int i = 0; i < segments; ++i) {
        int const j = (i + 1) % segments;
        idx.push_back(capT);
        idx.push_back(capT + 1 + static_cast<std::uint32_t>(i));
        idx.push_back(capT + 1 + static_cast<std::uint32_t>(j));
    }
}

void fillTerrainMeshCpu(GardenTerrain const& t, std::vector<VulkanRhi::Vertex>& vtx, std::vector<std::uint32_t>& idx) {
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
            Vec3 n{-dhx, 1.f, -dhz};
            float const nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (nl > 1e-6f) {
                n = n * (1.f / nl);
            } else {
                n = Vec3::unitY();
            }
            float const hn = (y - t.minHeight) / hSpan;
            Vec3 const col{
                0.14f + hn * 0.12f,
                0.28f + hn * 0.18f + std::fabs(dhx + dhz) * 0.04f,
                0.10f + hn * 0.07f,
            };
            pushVert(vtx, {wx, y, wz}, n, col);
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

[[nodiscard]] Vec3 colorForWallBrick(Aabb const& b) noexcept {
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

[[nodiscard]] Vec3 colorForKind(GardenColliderKind k, std::uint32_t salt) noexcept {
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
    case GardenColliderKind::Ground:
        return hashTint(salt, {0.22f, 0.38f, 0.16f}, 0.08f);
    case GardenColliderKind::Terrace:
        return hashTint(salt, {0.26f, 0.42f, 0.18f}, 0.09f);
    case GardenColliderKind::Wall:
        return hashTint(salt, {0.28f, 0.32f, 0.24f}, 0.07f);
    case GardenColliderKind::GrassBump:
        return hashTint(salt, {0.20f, 0.42f, 0.18f}, 0.1f);
    case GardenColliderKind::SandPit:
        return hashTint(salt, {0.32f, 0.28f, 0.18f}, 0.1f);
    case GardenColliderKind::Boulder:
        return hashTint(salt, {0.30f, 0.32f, 0.30f}, 0.06f);
    case GardenColliderKind::Rock:
        return hashTint(salt, {0.42f, 0.44f, 0.40f}, 0.055f);
    case GardenColliderKind::Leaf:
        return hashTint(salt, {0.14f, 0.42f, 0.16f}, 0.08f);
    case GardenColliderKind::Twig:
        return hashTint(salt, {0.24f, 0.16f, 0.10f}, 0.1f);
    case GardenColliderKind::Log:
        return hashTint(salt, {0.32f, 0.22f, 0.14f}, 0.07f);
    case GardenColliderKind::TwigScatter:
        return hashTint(salt, {0.22f, 0.15f, 0.10f}, 0.08f);
    case GardenColliderKind::TreeTrunk:
        return hashTint(salt, {0.22f, 0.14f, 0.08f}, 0.06f);
    case GardenColliderKind::TreeFoliage:
        return hashTint(salt, {0.10f, 0.36f, 0.12f}, 0.1f);
    }
    return {0.5f, 0.5f, 0.5f};
}

void worldRayFromWindowPixel(
    float mouseX,
    float mouseY,
    int fbW,
    int fbH,
    Vec3 eye,
    Vec3 target,
    float fovyRad,
    float aspect,
    Vec3& outOrigin,
    Vec3& outDir
) noexcept {
    outOrigin = eye;
    if (fbW < 1 || fbH < 1) {
        outDir = marble::math::normalize(target - eye);
        return;
    }
    float const ndcX = (2.f * mouseX / static_cast<float>(fbW)) - 1.f;
    float const ndcY = 1.f - (2.f * mouseY / static_cast<float>(fbH));
    Vec3 const f = marble::math::normalize(target - eye);
    Vec3 worldUp = Vec3::unitY();
    Vec3 r = marble::math::normalize(marble::math::cross(worldUp, f));
    if (marble::math::lengthSquared(r) < 1e-8f) {
        r = Vec3::unitX();
    }
    Vec3 const u = marble::math::cross(f, r);
    float const tanHalf = std::tan(fovyRad * 0.5f);
    Vec3 dir = f + r * (ndcX * tanHalf * aspect) + u * (ndcY * tanHalf);
    outDir = marble::math::normalize(dir);
}

/// Rough grounded test vs heightfield (props may read as airborne; good enough for jump gating).
[[nodiscard]] bool gardenBallOnGround(GardenLayout const& layout, RigidBodyKinematics const& ball, float radius) noexcept {
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

/// Easing for jump charge → impulse: shallow near 0 and 1, steepest growth ~70% charge (cubic bezier in t).
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

void applyLawnStrokeImpulse(
    RigidBodyKinematics& marble,
    Vec3 const& pStrokeStart,
    Vec3 const& pStrokeLast,
    Vec3 const& pStrokePrev,
    float flickVelEmaX,
    float flickVelEmaZ,
    bool flickEmaValid,
    float deltaSeconds
) noexcept {
    Vec3 const d = pStrokeLast - pStrokeStart;
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

    Vec3 dir{};
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
    applyImpulseLinear(marble, Vec3{dir.x * mag, 0.f, dir.z * mag});
}

} // namespace

struct GardenGame::State final {
    static constexpr std::size_t kPropMeshCount = static_cast<std::size_t>(marble::garden::GardenPropMesh::Count);

    core::Engine& engine;
    VulkanRhi rhi{};
    std::uint32_t meshTerrain = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t meshCube = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t meshSphere = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, kPropMeshCount> meshProps{};

    GardenLayout layout{};
    std::array<RigidBodyKinematics, 2> marbles{};
    SimplePhysicsWorld physics{};
    std::unique_ptr<marble::physics::IPhysicsScene> physicsScene_{createJoltPhysicsScene()};
    std::array<marble::physics::PhysicsBodyId, 2> marbleBodyIds_{};

    float camYaw = 0.7f;
    float camDist = 14.f;
    float camHeight = 3.6f;
    float camYawTarget = 0.7f;
    float camDistTarget = 14.f;
    float camHeightTarget = 3.6f;

    float strokeVelEmaX = 0.f;
    float strokeVelEmaZ = 0.f;
    bool strokeEmaValid = false;
    float padStrokeVelEmaX = 0.f;
    float padStrokeVelEmaZ = 0.f;
    bool padStrokeEmaValid = false;

    bool mouseWasDown = false;
    bool rightMouseWasDown = false;
    bool mouseFlickArmed = false;
    float strokePlaneY = 0.f;
    bool hasPlaneStroke = false;
    Vec3 pStrokeStart{};
    Vec3 pStrokeLast{};
    Vec3 pStrokePrev{};

    bool padWasDown = false;
    bool padFlickArmed = false;
    float padStrokePlaneY = 0.f;
    Vec3 padPStart{};
    Vec3 padPLast{};
    Vec3 padPPrev{};

    /// Accumulated seconds while jump input held on ground (capped); consumed on release.
    float jumpChargeSec_ = 0.f;
    bool jumpWasHeld_ = false;

    std::vector<MeshDrawInstance> drawScratch{};
    InputRemapTable inputRemap_{};
    ActionPolicy<256> inputPolicy_{};
    AbstractControlArray controlScratch_{};
    marble::core::BinaryResourceManager<64> assetRegistry_{};
    bool assetRegistryPrimed_{false};

    bool paused = false;
    PausePanel pausePanel = PausePanel::Main;
    bool pauseBackWasDown = false;
    bool menuRWasDown = false;
    bool menuTWasDown = false;
    bool menuOWasDown = false;
    bool menuQWasDown = false;
    bool padMenuAWasDown = false;
    bool padMenuXWasDown = false;
    bool padMenuBWasDown = false;
    bool padMenuYWasDown = false;

    static constexpr std::uint32_t kLayoutSeed = 0xC001D00u;

    void rebuildPhysicsFromLayout() noexcept {
        physicsScene_->clear();
        PhysicsBodyMaterial staticMat{};
        staticMat.restitution = 0.30f;
        staticMat.friction = 0.55f;
        GardenTerrain const& tr = layout.terrain;
        if (tr.sampleCount >= 2u && tr.heights.size() == static_cast<std::size_t>(tr.sampleCount) * tr.sampleCount) {
            PhysicsStaticHeightFieldDesc hf{};
            hf.offset = tr.origin;
            hf.scale = {tr.cellSize, 1.f, tr.cellSize};
            hf.sampleCount = tr.sampleCount;
            hf.heights = std::span<float const>(tr.heights.data(), tr.heights.size());
            hf.material = staticMat;
            (void)physicsScene_->addStaticHeightField(hf);
        }
        for (Aabb const& box : layout.staticColliders) {
            PhysicsStaticBoxDesc d{};
            d.bounds = box;
            d.material = staticMat;
            (void)physicsScene_->addStaticBox(d);
        }
        PhysicsBodyMaterial marbleMat{};
        marbleMat.restitution = 0.672f;
        marbleMat.friction = 0.42f;
        marbleMat.linearDamping = 0.02f;
        marbleMat.angularDamping = 0.10f;
        for (std::size_t i = 0; i < marbles.size(); ++i) {
            PhysicsDynamicSphereDesc sd{};
            sd.center = marbles[i].position;
            sd.linearVelocity = marbles[i].linearVelocity;
            sd.radius = kMarbleRadius;
            sd.invMass = marbles[i].invMass;
            sd.material = marbleMat;
            marbleBodyIds_[i] = physicsScene_->addDynamicSphere(sd);
        }
        physicsScene_->optimizeBroadPhase();
    }

    explicit State(core::Engine& e) : engine(e) {
        meshProps.fill(std::numeric_limits<std::uint32_t>::max());
        buildGardenLayout(kLayoutSeed, layout);
        placeMarblesInArena(marbles, layout);
        physics.setSettings({
            .gravity = {0.f, -9.81f, 0.f},
            .enableContinuousCollision = true,
            .maxSubSteps = 4u,
        });
        rebuildPhysicsFromLayout();

        (void)inputRemap_.bind(AbstractControl::BackSelect, {kActionPauseMenu, ControlValueClass::DigitalButton, false});

        inputPolicy_.resetActionGates();
        inputPolicy_.applyActionContext(std::span{kGameplayContext});
    }

    void primeAssetRegistryOnce() {
        if (assetRegistryPrimed_) {
            return;
        }
        assetRegistryPrimed_ = true;
        std::string const root = engine.assetsRootPath();
        if (root.empty()) {
            return;
        }
        (void)marble::core::setBinaryResourceSearchRoot(assetRegistry_, std::filesystem::path(root));
        (void)assetRegistry_.acquire("README.txt");
    }

    void restartGarden() noexcept {
        buildGardenLayout(kLayoutSeed, layout);
        placeMarblesInArena(marbles, layout);
        rebuildPhysicsFromLayout();
        camYaw = camYawTarget = 0.7f;
        camDist = camDistTarget = 14.f;
        camHeight = camHeightTarget = 3.6f;
        mouseFlickArmed = false;
        hasPlaneStroke = false;
        padFlickArmed = false;
        strokeEmaValid = false;
        padStrokeEmaValid = false;
        jumpChargeSec_ = 0.f;
        jumpWasHeld_ = false;
        paused = false;
        pausePanel = PausePanel::Main;
    }

    void step(core::Engine::FrameContext const& ctx) {
        primeAssetRegistryOnce();
        float const h = static_cast<float>(ctx.deltaSeconds);
        constexpr LogicalDevice kPlayer = LogicalDevice::Player;

        if (auto* w = engine.window()) {
            sampleKeyboardIntoAbstractControls(*w, controlScratch_);
            mergeAllConnectedGamepadsIntoAbstractControls(controlScratch_);

            bool const pauseHeld = actionScalar(kActionPauseMenu, controlScratch_, inputRemap_, inputPolicy_, kPlayer) >=
                    0.5f ||
                w->isKeyDown(Key::Escape);
            if (marble::game_shared::pauseMenuToggleEdge(pauseHeld, pauseBackWasDown)) {
                paused = !paused;
                pausePanel = PausePanel::Main;
            }

            if (paused) {
                if (!anyStandardGamepadPresent() && pausePanel == PausePanel::Options) {
                    pausePanel = PausePanel::Main;
                }
                bool const rDown = w->isKeyDown(Key::R);
                if (rDown && !menuRWasDown) {
                    restartGarden();
                }
                bool const oDown = w->isKeyDown(Key::O);
                if (anyStandardGamepadPresent() && oDown && !menuOWasDown) {
                    pausePanel = (pausePanel == PausePanel::Options) ? PausePanel::Main : PausePanel::Options;
                }
                menuRWasDown = rDown;
                menuOWasDown = oDown;

                std::size_t const iPadA = static_cast<std::size_t>(AbstractControl::RPadDown);
                std::size_t const iPadX = static_cast<std::size_t>(AbstractControl::RPadLeft);
                std::size_t const iPadB = static_cast<std::size_t>(AbstractControl::RPadRight);
                std::size_t const iPadY = static_cast<std::size_t>(AbstractControl::RPadUp);
                bool const qDown = w->isKeyDown(Key::Q);
                bool const aDown = controlScratch_[iPadA] >= 0.5f;
                bool const xDown = controlScratch_[iPadX] >= 0.5f;
                bool const bDown = controlScratch_[iPadB] >= 0.5f;
                bool const yDown = controlScratch_[iPadY] >= 0.5f;
                if (marble::game_shared::pauseMenuWantsReturnToLauncher(qDown, bDown, menuQWasDown, padMenuBWasDown)) {
                    engine.requestEndRun();
                }
                if (aDown && !padMenuAWasDown) {
                    paused = false;
                    pausePanel = PausePanel::Main;
                }
                if (xDown && !padMenuXWasDown) {
                    restartGarden();
                }
                if (anyStandardGamepadPresent() && yDown && !padMenuYWasDown) {
                    pausePanel = (pausePanel == PausePanel::Options) ? PausePanel::Main : PausePanel::Options;
                }
                padMenuAWasDown = aDown;
                padMenuXWasDown = xDown;
                padMenuBWasDown = bDown;
                padMenuYWasDown = yDown;

                bool const mouseDown = w->isMouseButtonDown(MouseButton::Left);
                bool const rightDown = w->isMouseButtonDown(MouseButton::Right);
                std::size_t const iRbPause = static_cast<std::size_t>(AbstractControl::RShoulder);
                bool const padRbDown = controlScratch_[iRbPause] >= 0.5f;
                mouseWasDown = mouseDown;
                rightMouseWasDown = rightDown;
                padWasDown = padRbDown;
            } else {
                menuRWasDown = w->isKeyDown(Key::R);
                menuTWasDown = w->isKeyDown(Key::T);
                menuOWasDown = w->isKeyDown(Key::O);
                padMenuAWasDown = controlScratch_[static_cast<std::size_t>(AbstractControl::RPadDown)] >= 0.5f;
                padMenuXWasDown = controlScratch_[static_cast<std::size_t>(AbstractControl::RPadLeft)] >= 0.5f;
                padMenuYWasDown = controlScratch_[static_cast<std::size_t>(AbstractControl::RPadUp)] >= 0.5f;
                marble::game_shared::syncPauseMenuReturnEdgeState(
                    w->isKeyDown(Key::Q),
                    controlScratch_[static_cast<std::size_t>(AbstractControl::RPadRight)] >= 0.5f,
                    menuQWasDown,
                    padMenuBWasDown);

            std::size_t const iLt = static_cast<std::size_t>(AbstractControl::LTrigger);
            std::size_t const iRt = static_cast<std::size_t>(AbstractControl::RTrigger);
            std::size_t const iRx = static_cast<std::size_t>(AbstractControl::RStickX);
            std::size_t const iRy = static_cast<std::size_t>(AbstractControl::RStickY);
            std::size_t const iRb = static_cast<std::size_t>(AbstractControl::RShoulder);
            std::size_t const iLx = static_cast<std::size_t>(AbstractControl::LStickX);
            std::size_t const iLy = static_cast<std::size_t>(AbstractControl::LStickY);
            bool const rbHeld = controlScratch_[iRb] >= 0.5f;

            float yawIn = 0.f;
            if (w->isKeyDown(Key::Right)) {
                yawIn += 1.f;
            }
            if (w->isKeyDown(Key::Left)) {
                yawIn -= 1.f;
            }
            if (!rbHeld) {
                yawIn += controlScratch_[iRx] * 1.25f;
            }
            float zoomIn = 0.f;
            if (w->isKeyDown(Key::Up)) {
                zoomIn += 1.f;
            }
            if (w->isKeyDown(Key::Down)) {
                zoomIn -= 1.f;
            }
            if (!rbHeld) {
                zoomIn += controlScratch_[iRy] * 1.05f;
            }
            constexpr float yawSpeed = 1.75f;
            constexpr float zoomSpeed = 2.05f;
            camYawTarget += yawIn * yawSpeed * h;
            camDistTarget -= zoomIn * zoomSpeed * h;
            camDistTarget = std::clamp(camDistTarget, 0.72f, 2.4f);

            if (w->isKeyDown(Key::E)) {
                camHeightTarget += 0.95f * h;
            }
            if (w->isKeyDown(Key::C)) {
                camHeightTarget -= 0.95f * h;
            }
            camHeightTarget = std::clamp(camHeightTarget, 0.15f, 1.4f);

            camHeightTarget += (controlScratch_[iRt] - controlScratch_[iLt]) * 0.58f * h;
            camHeightTarget = std::clamp(camHeightTarget, 0.15f, 1.4f);

            bool const mouseDown = w->isMouseButtonDown(MouseButton::Left);
            bool const rightDown = w->isMouseButtonDown(MouseButton::Right);
            double mx = 0.0, my = 0.0;
            (void)w->getCursorPos(mx, my);

            constexpr float kCamTau = 11.5f;
            float const camBlend = 1.f - std::exp(-kCamTau * h);
            camYaw += (camYawTarget - camYaw) * camBlend;
            camDist += (camDistTarget - camDist) * camBlend;
            camHeight += (camHeightTarget - camHeight) * camBlend;

            float const sxRoll = std::sin(camYaw);
            float const czRoll = std::cos(camYaw);
            Vec3 const worldFwdRoll{-sxRoll, 0.f, -czRoll};
            Vec3 const worldRightRoll{-czRoll, 0.f, sxRoll};
            float ax = 0.f;
            float az = 0.f;
            if (w->isKeyDown(Key::W)) {
                az += 1.f;
            }
            if (w->isKeyDown(Key::S)) {
                az -= 1.f;
            }
            if (w->isKeyDown(Key::D)) {
                ax += 1.f;
            }
            if (w->isKeyDown(Key::A)) {
                ax -= 1.f;
            }
            ax += controlScratch_[iLx];
            az -= controlScratch_[iLy];
            float const alen = std::sqrt(ax * ax + az * az);
            if (alen > 1e-5f && marbles[0].invMass > 0.f) {
                Vec3 wish = worldRightRoll * (ax / alen) + worldFwdRoll * (az / alen);
                constexpr float kMarbleRoll = 4.6f;
                applyImpulseLinear(marbles[0], wish * (kMarbleRoll * h));
                float const vx = marbles[0].linearVelocity.x;
                float const vz = marbles[0].linearVelocity.z;
                float const vh = std::sqrt(vx * vx + vz * vz);
                constexpr float kMaxHoriz = 5.2f;
                if (vh > kMaxHoriz && vh > 1e-6f) {
                    float const s = kMaxHoriz / vh;
                    marbles[0].linearVelocity.x *= s;
                    marbles[0].linearVelocity.z *= s;
                }
            }

            bool const spaceDown = w->isKeyDown(Key::Space);
            std::size_t const iPadA = static_cast<std::size_t>(AbstractControl::RPadDown);
            bool const padJumpDown = controlScratch_[iPadA] >= 0.5f;
            bool const jumpHeld = spaceDown || padJumpDown;
            bool const groundedForJump = gardenBallOnGround(layout, marbles[0], kMarbleRadius);
            constexpr float kJumpChargeMaxSec = 0.42f;
            constexpr float kJumpImpulseMin = 1.55f;
            constexpr float kJumpImpulseMax = 4.85f;
            // Charge builds whenever jump is held (air or ground); impulse only on release if grounded.
            if (jumpHeld) {
                jumpChargeSec_ += h;
                jumpChargeSec_ = std::min(jumpChargeSec_, kJumpChargeMaxSec);
            } else {
                if (jumpWasHeld_ && marbles[0].invMass > 0.f && jumpChargeSec_ > 1e-4f && groundedForJump) {
                    float const t = std::clamp(jumpChargeSec_ / kJumpChargeMaxSec, 0.f, 1.f);
                    float const s = jumpImpulseEase(t);
                    float const imp = kJumpImpulseMin + (kJumpImpulseMax - kJumpImpulseMin) * s;
                    applyImpulseLinear(marbles[0], Vec3{0.f, imp, 0.f});
                }
                jumpChargeSec_ = 0.f;
            }
            jumpWasHeld_ = jumpHeld;

            int fbW = 1, fbH = 1;
            w->getFramebufferSize(&fbW, &fbH);
            float const aspect =
                static_cast<float>(fbW) / static_cast<float>(std::max(1, fbH));
            constexpr float fovy = 60.f * 3.14159265f / 180.f;

            Vec3 const player = marbles[0].position;
            float const sx = std::sin(camYaw);
            float const cz = std::cos(camYaw);
            Vec3 const eye = player + Vec3{sx * camDist, camHeight, cz * camDist};
            Vec3 const target = player + Vec3{0.f, 0.06f, 0.f};

            if (mouseFlickArmed && rightDown && !rightMouseWasDown) {
                mouseFlickArmed = false;
                hasPlaneStroke = false;
            }

            if (mouseDown && !mouseWasDown) {
                Vec3 rayO{};
                Vec3 rayD{};
                worldRayFromWindowPixel(
                    static_cast<float>(mx),
                    static_cast<float>(my),
                    fbW,
                    fbH,
                    eye,
                    target,
                    fovy,
                    aspect,
                    rayO,
                    rayD
                );
                float tHit = 0.f;
                if (rayHitsSphere(rayO, rayD, marbles[0].position, kMarbleRadius * 1.15f, tHit)) {
                    strokePlaneY = marbles[0].position.y;
                    Vec3 pHit{};
                    if (!rayIntersectHorizontalPlane(rayO, rayD, strokePlaneY, pHit)) {
                        pHit = Vec3{marbles[0].position.x, strokePlaneY, marbles[0].position.z};
                    }
                    pStrokeStart = pHit;
                    pStrokeLast = pHit;
                    pStrokePrev = pHit;
                    hasPlaneStroke = true;
                    mouseFlickArmed = true;
                    strokeEmaValid = false;
                }
            }

            if (mouseFlickArmed && mouseDown) {
                Vec3 rayO{};
                Vec3 rayD{};
                worldRayFromWindowPixel(
                    static_cast<float>(mx),
                    static_cast<float>(my),
                    fbW,
                    fbH,
                    eye,
                    target,
                    fovy,
                    aspect,
                    rayO,
                    rayD
                );
                Vec3 p{};
                if (rayIntersectHorizontalPlane(rayO, rayD, strokePlaneY, p)) {
                    Vec3 const dStep = p - pStrokeLast;
                    float const planar = std::sqrt(dStep.x * dStep.x + dStep.z * dStep.z);
                    if (planar > 1e-7f) {
                        float const invDt = 1.f / std::max(h, 1e-4f);
                        float const ivx = (p.x - pStrokeLast.x) * invDt;
                        float const ivz = (p.z - pStrokeLast.z) * invDt;
                        constexpr float kStrokeEma = 0.38f;
                        if (!strokeEmaValid) {
                            strokeVelEmaX = ivx;
                            strokeVelEmaZ = ivz;
                            strokeEmaValid = true;
                        } else {
                            strokeVelEmaX = strokeVelEmaX * (1.f - kStrokeEma) + ivx * kStrokeEma;
                            strokeVelEmaZ = strokeVelEmaZ * (1.f - kStrokeEma) + ivz * kStrokeEma;
                        }
                    }
                    pStrokePrev = pStrokeLast;
                    pStrokeLast = p;
                }
            }

            if (mouseWasDown && !mouseDown && mouseFlickArmed) {
                if (hasPlaneStroke) {
                    applyLawnStrokeImpulse(
                        marbles[0],
                        pStrokeStart,
                        pStrokeLast,
                        pStrokePrev,
                        strokeVelEmaX,
                        strokeVelEmaZ,
                        strokeEmaValid,
                        h
                    );
                }
                mouseFlickArmed = false;
                hasPlaneStroke = false;
                strokeEmaValid = false;
            }

            bool const padDown = controlScratch_[iRb] >= 0.5f;
            constexpr float kPadStickScale = 3.55f;
            Vec3 const worldFwd{-sx, 0.f, -cz};
            Vec3 const worldRight{-cz, 0.f, sx};

            if (padDown && !padWasDown) {
                padStrokePlaneY = marbles[0].position.y;
                padPStart = Vec3{marbles[0].position.x, padStrokePlaneY, marbles[0].position.z};
                padPLast = padPStart;
                padPPrev = padPStart;
                padFlickArmed = true;
                padStrokeEmaValid = false;
            }
            if (padFlickArmed && padDown) {
                float rx = controlScratch_[iRx];
                float ry = controlScratch_[iRy];
                float const rm = std::sqrt(rx * rx + ry * ry);
                if (rm > 1e-5f) {
                    rx /= rm;
                    ry /= rm;
                }
                Vec3 const delta =
                    worldRight * (rx * kPadStickScale * h) + worldFwd * ((-ry) * kPadStickScale * h);
                Vec3 const next = padPLast + delta;
                Vec3 const dpad = next - padPLast;
                float const planarPad = std::sqrt(dpad.x * dpad.x + dpad.z * dpad.z);
                if (planarPad > 1e-7f) {
                    float const invDt = 1.f / std::max(h, 1e-4f);
                    float const ivx = (next.x - padPLast.x) * invDt;
                    float const ivz = (next.z - padPLast.z) * invDt;
                    constexpr float kPadEma = 0.38f;
                    if (!padStrokeEmaValid) {
                        padStrokeVelEmaX = ivx;
                        padStrokeVelEmaZ = ivz;
                        padStrokeEmaValid = true;
                    } else {
                        padStrokeVelEmaX = padStrokeVelEmaX * (1.f - kPadEma) + ivx * kPadEma;
                        padStrokeVelEmaZ = padStrokeVelEmaZ * (1.f - kPadEma) + ivz * kPadEma;
                    }
                }
                padPPrev = padPLast;
                padPLast = next;
                padPLast.y = padStrokePlaneY;
            }
            if (padWasDown && !padDown && padFlickArmed) {
                applyLawnStrokeImpulse(
                    marbles[0],
                    padPStart,
                    padPLast,
                    padPPrev,
                    padStrokeVelEmaX,
                    padStrokeVelEmaZ,
                    padStrokeEmaValid,
                    h
                );
                padFlickArmed = false;
                padStrokeEmaValid = false;
            }

            mouseWasDown = mouseDown;
            rightMouseWasDown = rightDown;
            padWasDown = padDown;
            }
        }

        if (!paused) {
            float const minCenterY = layout.terrain.minHeight - kMarbleRadius - 0.55f;
            PhysicsCylindricalXZClamp const clamp{
                kGardenRadius - kMarbleRadius - 0.02f,
                minCenterY,
            };
            PhysicsStepOptions const stepOpts{&clamp, std::span(marbleBodyIds_)};
            physicsScene_->syncHostVelocitiesBeforeStep(marbleBodyIds_, marbles.data(), marbles.size());
            physicsScene_->step(h, physics.settings(), stepOpts);
            physicsScene_->readBackKinematics(marbleBodyIds_, marbles.data(), marbles.size());
        }

        if (auto* w = engine.window()) {
            char buf[256];
            if (paused) {
                if (pausePanel == PausePanel::Options) {
                    (void)std::snprintf(
                        buf,
                        sizeof(buf),
                        "Garden — Paused — Options — Esc · O back · R · Q menu");
                } else {
                    (void)std::snprintf(
                        buf,
                        sizeof(buf),
                        "Garden — Paused — Esc resume · O options · R restart · Q menu");
                }
            } else {
                (void)std::snprintf(
                    buf,
                    sizeof(buf),
                    "Garden — Esc · WASD roll · arrows camera · Space jump · E/C height · click flick");
            }
            w->setTitle(buf);
        }
    }

    void renderFrame(core::Engine::FrameContext const& ctx) {
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

        int fbW = 1, fbH = 1;
        if (auto* w = engine.window()) {
            w->getFramebufferSize(&fbW, &fbH);
        }
        float const aspect = static_cast<float>(fbW) / static_cast<float>(std::max(1, fbH));
        constexpr float fovy = 60.f * 3.14159265f / 180.f;

        Vec3 const player = marbles[0].position;
        float const sx = std::sin(camYaw);
        float const cz = std::cos(camYaw);
        Vec3 const eye = player + Vec3{sx * camDist, camHeight, cz * camDist};
        Vec3 const target = player + Vec3{0.f, 0.06f, 0.f};
        Mat4 const view = lookAtLh(eye, target, Vec3::unitY());
        Mat4 const proj = perspectiveVulkan(fovy, aspect, 0.12f, 420.f);
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
            // Unit mesh in [-0.5,0.5]³; `ext` is layout half-extent — same convention as `buildGardenLayout`.
            Vec3 const modelOrigin = ctr;
            Mat4 const model = Mat4::translation(modelOrigin) * rot * Mat4::scaling(ext * 2.f);
            pushMesh(meshProps[mid], model, col);
        }

        for (std::size_t mi = 0; mi < marbles.size(); ++mi) {
            MeshDrawInstance d{};
            d.meshIndex = meshSphere;
            d.model = Mat4::translation(marbles[mi].position) * Mat4::scaling({kMarbleRadius, kMarbleRadius, kMarbleRadius});
            d.color = mi == 0 ? Vec3{0.52f, 0.78f, 0.95f} : Vec3{0.82f, 0.88f, 0.92f};
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
};

class RenderPhase final : public core::Engine::IRenderPhase {
public:
    explicit RenderPhase(GardenGame::State* s) : state_(s) {}

    void render(core::Engine::FrameContext const& ctx) override {
        if (state_) {
            state_->renderFrame(ctx);
        }
    }

private:
    GardenGame::State* state_;
};

GardenGame::GardenGame(core::Engine& engine) : engine_(engine), state_(std::make_unique<State>(engine)) {}

GardenGame::~GardenGame() {
    if (state_) {
        state_->rhi.shutdown();
    }
    engine_.resetPhasesToDefaults();
}

void GardenGame::installPhases() {
    auto* raw = state_.get();
    auto rend = std::make_unique<RenderPhase>(raw);
    auto fixed = std::make_unique<core::FixedStepSimulationPhase>();
    fixed->setStepCallback([raw](core::Engine::FrameContext const& ctx) { raw->step(ctx); });
    engine_.setSimulationPhase(std::move(fixed));
    engine_.setRenderPhase(std::move(rend));
}

bool GardenGame::initGraphics(std::string shaderDirectory, std::optional<std::uint32_t> physicalDeviceIndex) {
    if (!engine_.window()) {
        return false;
    }
    bool vkOk = false;
    std::string const assetsRoot = engine_.assetsRootPath();
    if (!assetsRoot.empty()) {
        (void)marble::core::setBinaryResourceSearchRoot(state_->assetRegistry_, std::filesystem::path(assetsRoot));
        if (state_->assetRegistry_.acquire("shaders/mesh.vert.spv") &&
            state_->assetRegistry_.acquire("shaders/mesh.frag.spv")) {
            marble::core::BinaryResource const* const vertRes = state_->assetRegistry_.find("shaders/mesh.vert.spv");
            marble::core::BinaryResource const* const fragRes = state_->assetRegistry_.find("shaders/mesh.frag.spv");
            if (vertRes != nullptr && fragRes != nullptr) {
                std::span<std::uint8_t const> const vspan(vertRes->bytes.data(), vertRes->bytes.size());
                std::span<std::uint8_t const> const fspan(fragRes->bytes.data(), fragRes->bytes.size());
                vkOk = state_->rhi.initFromSpirvBytes(
                    *engine_.window(), "Garden", vspan, fspan, physicalDeviceIndex);
            }
        }
    }
    if (!vkOk) {
        if (!state_->rhi.init(*engine_.window(), "Garden", std::move(shaderDirectory), physicalDeviceIndex)) {
            return false;
        }
    }
    IRenderBackend& rb = state_->rhi;
    rb.setClearColor(0.07f, 0.16f, 0.19f, 1.f);

    std::vector<VulkanRhi::Vertex> terrVx;
    std::vector<std::uint32_t> terrIx;
    fillTerrainMeshCpu(state_->layout.terrain, terrVx, terrIx);
    state_->meshTerrain = state_->rhi.uploadMesh(terrVx, terrIx);
    if (state_->meshTerrain == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<VulkanRhi::Vertex> cv;
    std::vector<std::uint32_t> ci;
    addCube(cv, ci, {1.f, 1.f, 1.f});
    state_->meshCube = state_->rhi.uploadMesh(cv, ci);
    if (state_->meshCube == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    using marble::garden::GardenPropMesh;
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Cube)] = state_->meshCube;

    std::vector<VulkanRhi::Vertex> ov;
    std::vector<std::uint32_t> oi;
    addOctahedron(ov, oi, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Octahedron)] = state_->rhi.uploadMesh(ov, oi);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Octahedron)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<VulkanRhi::Vertex> iv;
    std::vector<std::uint32_t> ii;
    addIcosahedronSubdividedClean(iv, ii, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Icosahedron)] = state_->rhi.uploadMesh(iv, ii);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Icosahedron)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Foliage)] =
        state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Icosahedron)];

    std::vector<VulkanRhi::Vertex> dv;
    std::vector<std::uint32_t> di;
    addDiscExtrudedY(dv, di, 20, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Disc)] = state_->rhi.uploadMesh(dv, di);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::Disc)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<VulkanRhi::Vertex> tv;
    std::vector<std::uint32_t> ti;
    addCylinderAlongX(tv, ti, 16, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TwigCapsule)] = state_->rhi.uploadMesh(tv, ti);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TwigCapsule)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<VulkanRhi::Vertex> yv;
    std::vector<std::uint32_t> yi;
    addCylinderAlongY(yv, yi, 14, {1.f, 1.f, 1.f});
    state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TrunkY)] = state_->rhi.uploadMesh(yv, yi);
    if (state_->meshProps[static_cast<std::size_t>(GardenPropMesh::TrunkY)] ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<VulkanRhi::Vertex> sv;
    std::vector<std::uint32_t> si;
    addUvSphere(sv, si, 1.f, 16, 24, {1.f, 1.f, 1.f});
    state_->meshSphere = state_->rhi.uploadMesh(sv, si);
    if (state_->meshSphere == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    return true;
}

} // namespace marble::garden_app
