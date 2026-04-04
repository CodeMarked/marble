#include "gameplay/GameplayFoundation.hpp"

int main() {
    using namespace marble;

    gameplay::GameplayWorld<3, 8> world{};

    if (!world.spawn(1u, 10u, math::Vec3{1.f, 2.f, 3.f})) {
        return 1;
    }
    if (!world.spawn(2u, 20u, math::Vec3{4.f, 5.f, 6.f})) {
        return 2;
    }
    if (world.activeCount() != 2u) {
        return 3;
    }

    auto* o1 = world.findMutable(1u);
    if (o1 == nullptr || o1->archetypeId != 10u) {
        return 4;
    }

    // Update same id in place.
    if (!world.spawn(1u, 11u, math::Vec3{7.f, 8.f, 9.f})) {
        return 5;
    }
    o1 = world.findMutable(1u);
    if (o1 == nullptr || o1->archetypeId != 11u || o1->position.x != 7.f) {
        return 6;
    }

    if (!world.spawn(3u, 30u, math::Vec3{})) {
        return 7;
    }
    if (world.spawn(4u, 40u, math::Vec3{})) {
        return 8;
    }

    int activeEnabledCount = 0;
    world.forEachActive([&activeEnabledCount](gameplay::GameplayObject&) { ++activeEnabledCount; });
    if (activeEnabledCount != 3) {
        return 9;
    }

    if (!world.queueEditorCommand({gameplay::EditorCommandType::ToggleEnabled, 2u, 0u, {}, false})) {
        return 10;
    }
    if (!world.queueEditorCommand({gameplay::EditorCommandType::MoveObject, 1u, 0u, {10.f, 0.f, 0.f}, true})) {
        return 11;
    }
    if (!world.queueEditorCommand({gameplay::EditorCommandType::RemoveObject, 3u, 0u, {}, true})) {
        return 12;
    }
    if (!world.queueEditorCommand({gameplay::EditorCommandType::AddObject, 4u, 40u, {2.f, 0.f, 0.f}, true})) {
        return 13;
    }

    world.applyQueuedEditorCommands();

    if (world.queuedEditorCommandCount() != 0u) {
        return 14;
    }
    if (world.activeCount() != 3u) {
        return 15;
    }
    auto* o2 = world.findMutable(2u);
    if (o2 == nullptr || o2->enabled) {
        return 16;
    }
    o1 = world.findMutable(1u);
    if (o1 == nullptr || o1->position.x != 10.f) {
        return 17;
    }
    if (world.findMutable(3u) != nullptr) {
        return 18;
    }
    if (world.findMutable(4u) == nullptr) {
        return 19;
    }

    // Disabled object should be skipped.
    activeEnabledCount = 0;
    world.forEachActive([&activeEnabledCount](gameplay::GameplayObject&) { ++activeEnabledCount; });
    if (activeEnabledCount != 2) {
        return 20;
    }

    return 0;
}
