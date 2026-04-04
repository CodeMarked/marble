#include "gameplay/RuntimeObjectModel.hpp"

int main() {
    using namespace marble;

    gameplay::RuntimeObjectStore<2> store{};
    if (store.capacity() != 2u || store.aliveCount() != 0u || store.full()) {
        return 1;
    }

    const gameplay::ObjectHandle a = store.create({10u, {1.f, 2.f, 3.f}, true});
    if (!store.isAlive(a) || store.aliveCount() != 1u) {
        return 2;
    }
    const gameplay::ObjectHandle b = store.create({20u, {4.f, 5.f, 6.f}, false});
    if (!store.isAlive(b) || !store.full() || store.aliveCount() != 2u) {
        return 3;
    }

    // Over-capacity create keeps store unchanged.
    (void)store.create({30u, {}, true});
    if (store.aliveCount() != 2u || !store.full()) {
        return 4;
    }

    auto* pa = store.getMutable(a);
    if (pa == nullptr || pa->archetypeId != 10u || pa->position.y != 2.f) {
        return 5;
    }
    pa->enabled = false;
    if (store.get(a)->enabled) {
        return 6;
    }

    int iterCount = 0;
    store.forEachAlive([&iterCount](gameplay::ObjectHandle, gameplay::RuntimeObject&) { ++iterCount; });
    if (iterCount != 2) {
        return 7;
    }

    if (!store.destroy(a) || store.isAlive(a) || store.aliveCount() != 1u) {
        return 8;
    }
    if (store.destroy(a)) {
        return 9;
    }

    // Slot reuse must bump generation; old handle remains invalid.
    const gameplay::ObjectHandle c = store.create({11u, {7.f, 8.f, 9.f}, true});
    if (!store.isAlive(c) || c.index != a.index || c.generation == a.generation) {
        return 10;
    }
    if (store.isAlive(a)) {
        return 11;
    }

    return 0;
}
