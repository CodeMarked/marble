#pragma once

#include "math/Vec3.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace marble::render {

inline void pushVert(
    std::vector<VulkanRhi::Vertex>& vtx,
    marble::math::Vec3 p,
    marble::math::Vec3 n,
    marble::math::Vec3 color
) {
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

inline void addCube(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    marble::math::Vec3 color
) {
    auto face = [&](marble::math::Vec3 n, marble::math::Vec3 t0, marble::math::Vec3 t1, marble::math::Vec3 t2,
                     marble::math::Vec3 t3) {
        std::uint32_t base = static_cast<std::uint32_t>(vtx.size());
        auto push = [&](marble::math::Vec3 p) {
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

inline void addUvSphere(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    float radius,
    int stacks,
    int slices,
    marble::math::Vec3 color
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
            int const a = static_cast<int>(base) + i * stride + j;
            int const b = static_cast<int>(base) + (i + 1) * stride + j;
            idx.push_back(static_cast<std::uint32_t>(a));
            idx.push_back(static_cast<std::uint32_t>(b));
            idx.push_back(static_cast<std::uint32_t>(a + 1));
            idx.push_back(static_cast<std::uint32_t>(a + 1));
            idx.push_back(static_cast<std::uint32_t>(b));
            idx.push_back(static_cast<std::uint32_t>(b + 1));
        }
    }
}

inline void addOctahedron(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    marble::math::Vec3 color
) {
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
        marble::math::Vec3 const a{vtx[o + static_cast<std::uint32_t>(f[0])].px, vtx[o + static_cast<std::uint32_t>(f[0])].py,
            vtx[o + static_cast<std::uint32_t>(f[0])].pz};
        marble::math::Vec3 const b{vtx[o + static_cast<std::uint32_t>(f[1])].px, vtx[o + static_cast<std::uint32_t>(f[1])].py,
            vtx[o + static_cast<std::uint32_t>(f[1])].pz};
        marble::math::Vec3 const c3{vtx[o + static_cast<std::uint32_t>(f[2])].px, vtx[o + static_cast<std::uint32_t>(f[2])].py,
            vtx[o + static_cast<std::uint32_t>(f[2])].pz};
        marble::math::Vec3 e0 = b - a;
        marble::math::Vec3 e1 = c3 - a;
        marble::math::Vec3 faceNormal = marble::math::cross(e0, e1);
        float const ln = std::sqrt(marble::math::lengthSquared(faceNormal));
        if (ln > 1e-8f) {
            faceNormal = faceNormal * (1.f / ln);
        } else {
            faceNormal = {0.f, 1.f, 0.f};
        }
        for (int k = 0; k < 3; ++k) {
            vtx[o + static_cast<std::uint32_t>(f[k])].nx = faceNormal.x;
            vtx[o + static_cast<std::uint32_t>(f[k])].ny = faceNormal.y;
            vtx[o + static_cast<std::uint32_t>(f[k])].nz = faceNormal.z;
        }
        idx.push_back(o + static_cast<std::uint32_t>(f[0]));
        idx.push_back(o + static_cast<std::uint32_t>(f[1]));
        idx.push_back(o + static_cast<std::uint32_t>(f[2]));
    }
}

/// One level of midpoint subdivision on the unit icosa (≈80 faces), projected back onto the sphere — no noise.
inline void addIcosahedronSubdividedClean(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    marble::math::Vec3 color
) {
    float const phi = (1.f + std::sqrt(5.f)) * 0.5f;
    std::vector<marble::math::Vec3> p = {
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
    for (marble::math::Vec3& v : p) {
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
        marble::math::Vec3 const pa = p[a];
        marble::math::Vec3 const pb = p[b];
        marble::math::Vec3 m = (pa + pb) * 0.5f;
        float const lm = std::sqrt(marble::math::lengthSquared(m));
        if (lm > 1e-8f) {
            m = m * (0.5f / lm);
        } else {
            m = marble::math::Vec3{0.f, 0.5f, 0.f};
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
        marble::math::Vec3 const va = p[t[0]];
        marble::math::Vec3 const vb = p[t[1]];
        marble::math::Vec3 const vc = p[t[2]];
        marble::math::Vec3 e0 = vb - va;
        marble::math::Vec3 e1 = vc - va;
        marble::math::Vec3 fn = marble::math::cross(e0, e1);
        float const ln = std::sqrt(marble::math::lengthSquared(fn));
        if (ln > 1e-8f) {
            fn = fn * (1.f / ln);
        } else {
            fn = marble::math::Vec3{0.f, 1.f, 0.f};
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

/// Closed “puck” in XZ (radius 0.5), height 1 along Y from -0.5..0.5 — scales to a solid sand slab, not a single sheet.
inline void addDiscExtrudedY(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    int segments,
    marble::math::Vec3 color
) {
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
        marble::math::Vec3 const n{nx, 0.f, nz};
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
inline void addCylinderAlongX(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    int segments,
    marble::math::Vec3 color
) {
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
        marble::math::Vec3 n{0.f, std::cos(ang), std::sin(ang)};
        pushVert(vtx, {x0, y, z}, n, color);
    }
    std::uint32_t const ring1 = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const y = std::cos(ang) * r;
        float const z = std::sin(ang) * r;
        marble::math::Vec3 n{0.f, std::cos(ang), std::sin(ang)};
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
inline void addCylinderAlongY(
    std::vector<VulkanRhi::Vertex>& vtx,
    std::vector<std::uint32_t>& idx,
    int segments,
    marble::math::Vec3 color
) {
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
        marble::math::Vec3 const n{std::cos(ang), 0.f, std::sin(ang)};
        pushVert(vtx, {x, y0, z}, n, color);
    }
    std::uint32_t const ring1 = static_cast<std::uint32_t>(vtx.size());
    for (int i = 0; i < segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = t * 2.f * 3.14159265f;
        float const x = std::cos(ang) * r;
        float const z = std::sin(ang) * r;
        marble::math::Vec3 const n{std::cos(ang), 0.f, std::sin(ang)};
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

} // namespace marble::render
