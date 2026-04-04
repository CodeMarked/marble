#include "physics/CollisionMiddleware.hpp"

#include <cstdint>

int main() {
    using marble::math::Aabb;
    using marble::math::Point3;
    using marble::math::Sphere;
    using marble::physics::CollisionFilter;
    using marble::physics::filtersAllow;
    using marble::physics::intersects;

    {
        const Sphere a{{0.f, 0.f, 0.f}, 1.f};
        const Sphere b{{2.f, 0.f, 0.f}, 1.f};
        if (!intersects(a, b)) {
            return 1;
        }
        const Sphere c{{2.1f, 0.f, 0.f}, 1.f};
        if (intersects(a, c)) {
            return 2;
        }
    }

    {
        const Aabb x{{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};
        const Aabb y{{0.5f, 0.5f, 0.5f}, {2.f, 2.f, 2.f}};
        if (!intersects(x, y)) {
            return 3;
        }
        const Aabb z{{2.f, 2.f, 2.f}, {3.f, 3.f, 3.f}};
        if (intersects(x, z)) {
            return 4;
        }
    }

    constexpr std::uint32_t kLayerStatic = 1u << 0;
    constexpr std::uint32_t kLayerDynamic = 1u << 1;
    constexpr std::uint32_t kLayerPlayer = 1u << 2;

    const CollisionFilter staticBody{kLayerStatic, kLayerDynamic | kLayerPlayer};
    const CollisionFilter dynamicBody{kLayerDynamic, kLayerStatic | kLayerDynamic};
    const CollisionFilter player{kLayerPlayer, kLayerStatic};

    if (!filtersAllow(staticBody, dynamicBody) || !filtersAllow(staticBody, player)) {
        return 5;
    }
    if (filtersAllow(player, dynamicBody)) {
        return 6;
    }

    return 0;
}
