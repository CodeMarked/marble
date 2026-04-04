#include "gameplay/EventsAndMessaging.hpp"

#include "core/StringId.hpp"

namespace {

void accumulate(void* userData, marble::gameplay::EventPayload payload) noexcept {
    auto* total = static_cast<int*>(userData);
    *total += static_cast<int>(payload);
}

} // namespace

int main() {
    using marble::core::StringId;
    using namespace marble::core::literals;
    using namespace marble::gameplay;

    EventBus<4, 4> bus{};
    int a = 0;
    int b = 0;

    const StringId evtHit = "hit"_sid;
    const StringId evtSpawn = "spawn"_sid;

    if (!bus.subscribe(evtHit, &accumulate, &a)) {
        return 1;
    }
    if (!bus.subscribe(evtHit, &accumulate, &b)) {
        return 2;
    }
    if (bus.subscribe(evtHit, &accumulate, &a)) {
        return 3;
    }
    if (!bus.subscribe(evtSpawn, &accumulate, &a)) {
        return 4;
    }
    if (bus.subscribe(StringId{}, &accumulate, &a)) {
        return 5;
    }
    if (bus.subscribe(evtHit, nullptr, &a)) {
        return 6;
    }

    if (!bus.publish({evtHit, 3})) {
        return 7;
    }
    if (!bus.publish({evtSpawn, 10})) {
        return 8;
    }
    if (!bus.publish({evtHit, -2})) {
        return 9;
    }
    if (bus.publish({StringId{}, 1})) {
        return 10;
    }

    const std::size_t delivered = bus.dispatchAll();
    if (delivered != 5u) {
        return 11;
    }
    if (a != 11) { // hit: +3-2, spawn: +10
        return 12;
    }
    if (b != 1) { // hit only
        return 13;
    }
    if (bus.queuedEventCount() != 0u) {
        return 14;
    }

    if (bus.unsubscribe(evtHit, &accumulate, &b) != 1u) {
        return 15;
    }
    if (bus.unsubscribe(evtHit, &accumulate, &b) != 0u) {
        return 16;
    }

    if (!bus.publish({evtHit, 2})) {
        return 17;
    }
    if (bus.dispatchAll() != 1u) {
        return 18;
    }
    if (a != 13 || b != 1) {
        return 19;
    }

    EventBus<2, 2> small{};
    if (!small.subscribe("a"_sid, &accumulate, &a)) {
        return 20;
    }
    if (!small.subscribe("b"_sid, &accumulate, &a)) {
        return 21;
    }
    if (small.subscribe("c"_sid, &accumulate, &a)) {
        return 22;
    }
    if (!small.publish({"a"_sid, 1}) || !small.publish({"b"_sid, 1})) {
        return 23;
    }
    if (small.publish({"a"_sid, 1})) {
        return 24;
    }

    return 0;
}
