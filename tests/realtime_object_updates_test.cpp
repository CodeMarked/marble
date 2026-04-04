#include "gameplay/RealtimeObjectUpdates.hpp"

#include <array>

int main() {
    using namespace marble;
    using namespace marble::gameplay;

    RuntimeObjectStore<5> store{};
    const ObjectHandle h1 = store.create({1u, {0.f, 0.f, 0.f}, true});
    const ObjectHandle h2 = store.create({2u, {1.f, 0.f, 0.f}, false}); // disabled
    const ObjectHandle h3 = store.create({3u, {2.f, 0.f, 0.f}, true});

    std::array<ObjectHandle, 5> handles{};
    const std::size_t count = collectAliveHandles(store, handles.data(), handles.size());
    if (count != 3u) {
        return 1;
    }

    UpdateContext ctx{};
    ctx.deltaSeconds = 2.f;
    ctx.gravity = {0.f, -10.f, 0.f}; // delta = 0.5 * g * t^2 = -20 in Y

    integrateObjectPositions(store, handles.data(), count, ctx);

    const RuntimeObject* o1 = store.get(h1);
    const RuntimeObject* o2 = store.get(h2);
    const RuntimeObject* o3 = store.get(h3);
    if (o1 == nullptr || o2 == nullptr || o3 == nullptr) {
        return 2;
    }
    if (o1->position.y != -20.f || o3->position.y != -20.f) {
        return 3;
    }
    if (o2->position.y != 0.f) {
        return 4;
    }

    // Delta <= 0 or null list: no-op.
    const float yBefore = o1->position.y;
    integrateObjectPositions(store, handles.data(), count, UpdateContext{0.f, {0.f, -1.f, 0.f}});
    if (store.get(h1)->position.y != yBefore) {
        return 5;
    }
    integrateObjectPositions(store, nullptr, count, ctx);
    if (store.get(h1)->position.y != yBefore) {
        return 6;
    }

    std::array<UpdateWorkRange, 8> ranges{};
    const std::size_t r = buildUpdateWorkRanges(10u, 4u, ranges);
    if (r != 3u) {
        return 7;
    }
    if (ranges[0].begin != 0u || ranges[0].count != 4u) {
        return 8;
    }
    if (ranges[1].begin != 4u || ranges[1].count != 4u) {
        return 9;
    }
    if (ranges[2].begin != 8u || ranges[2].count != 2u) {
        return 10;
    }
    if (buildUpdateWorkRanges(10u, 0u, ranges) != 0u) {
        return 11;
    }

    return 0;
}
