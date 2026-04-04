#include "physics/CollisionDetection.hpp"

#include <array>
#include <cstdint>

namespace {

bool approx(float a, float b, float eps = 1e-4f) {
    return a - b <= eps && b - a <= eps;
}

} // namespace

int main() {
    using marble::math::Aabb;
    using marble::math::Ray3;
    using marble::math::Sphere;
    using marble::math::Vec3;
    using marble::physics::BroadphaseProxy;
    using marble::physics::collectAabbOverlapPairs;
    using marble::physics::rayAabbInterval;
    using marble::physics::raycastAabb;
    using marble::physics::raycastSphere;

    {
        const Ray3 ray{{2.f, 0.f, 0.f}, Vec3{-1.f, 0.f, 0.f}};
        const Sphere s{{0.f, 0.f, 0.f}, 1.f};
        float t{};
        if (!raycastSphere(ray, s, t) || !approx(t, 1.f)) {
            return 1;
        }
    }

    {
        const Ray3 ray{{5.f, 0.f, 0.f}, Vec3{1.f, 0.f, 0.f}};
        const Sphere s{{0.f, 0.f, 0.f}, 1.f};
        float t{};
        if (raycastSphere(ray, s, t)) {
            return 2;
        }
    }

    {
        const Ray3 ray{{-2.f, 0.5f, 0.5f}, Vec3{1.f, 0.f, 0.f}};
        const Aabb box{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};
        float t0{};
        float t1{};
        if (!rayAabbInterval(ray, box, t0, t1) || !approx(t0, 2.f) || !approx(t1, 3.f)) {
            return 3;
        }
        float th{};
        if (!raycastAabb(ray, box, th) || !approx(th, 2.f)) {
            return 4;
        }
    }

    {
        const Ray3 ray{{0.5f, 5.f, 0.5f}, Vec3{0.f, -1.f, 0.f}};
        const Aabb box{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};
        float th{};
        if (!raycastAabb(ray, box, th) || !approx(th, 4.f)) {
            return 5;
        }
    }

    std::array<BroadphaseProxy, 3> proxies{};
    proxies[0] = {10u, Aabb{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}}};
    proxies[1] = {20u, Aabb{{0.5f, 0.5f, 0.5f}, {2.f, 2.f, 2.f}}};
    proxies[2] = {30u, Aabb{{10.f, 10.f, 10.f}, {11.f, 11.f, 11.f}}};

    std::array<std::uint32_t, 4> a{};
    std::array<std::uint32_t, 4> b{};
    const std::size_t n = collectAabbOverlapPairs(proxies.data(), proxies.size(), a.data(), b.data(), a.size());
    if (n != 1u) {
        return 6;
    }
    if (!((a[0] == 10u && b[0] == 20u) || (a[0] == 20u && b[0] == 10u))) {
        return 7;
    }

    if (collectAabbOverlapPairs(nullptr, 1, a.data(), b.data(), 1) != 0u) {
        return 8;
    }

    return 0;
}
