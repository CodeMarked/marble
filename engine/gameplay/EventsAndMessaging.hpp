#pragma once

#include "core/StringId.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

using EventPayload = std::int32_t;
using EventHandler = void (*)(void* userData, EventPayload payload);

struct EventMessage {
    core::StringId id{};
    EventPayload payload{};
};

struct EventSubscription {
    core::StringId id{};
    EventHandler handler{};
    void* userData{};
};

template <std::size_t MaxSubscriptions, std::size_t MaxQueuedEvents>
class EventBus {
public:
    static_assert(MaxSubscriptions > 0, "EventBus requires positive subscription capacity");
    static_assert(MaxQueuedEvents > 0, "EventBus requires positive queue capacity");

    [[nodiscard]] bool subscribe(core::StringId id, EventHandler handler, void* userData) noexcept {
        if (id.value == 0u || handler == nullptr || subCount_ >= MaxSubscriptions) {
            return false;
        }
        for (std::size_t i = 0; i < subCount_; ++i) {
            EventSubscription const& s = subscriptions_[i];
            if (s.id == id && s.handler == handler && s.userData == userData) {
                return false;
            }
        }
        subscriptions_[subCount_++] = EventSubscription{id, handler, userData};
        return true;
    }

    [[nodiscard]] std::size_t unsubscribe(core::StringId id, EventHandler handler, void* userData) noexcept {
        std::size_t removed = 0;
        for (std::size_t i = 0; i < subCount_;) {
            EventSubscription const& s = subscriptions_[i];
            if (s.id == id && s.handler == handler && s.userData == userData) {
                subscriptions_[i] = subscriptions_[subCount_ - 1u];
                --subCount_;
                ++removed;
                continue;
            }
            ++i;
        }
        return removed;
    }

    [[nodiscard]] bool publish(EventMessage const& event) noexcept {
        if (event.id.value == 0u || queuedCount_ >= MaxQueuedEvents) {
            return false;
        }
        queued_[queuedCount_++] = event;
        return true;
    }

    [[nodiscard]] std::size_t dispatchAll() noexcept {
        std::size_t delivered = 0;
        for (std::size_t i = 0; i < queuedCount_; ++i) {
            EventMessage const e = queued_[i];
            for (std::size_t j = 0; j < subCount_; ++j) {
                EventSubscription const& s = subscriptions_[j];
                if (s.id != e.id) {
                    continue;
                }
                s.handler(s.userData, e.payload);
                ++delivered;
            }
        }
        queuedCount_ = 0;
        return delivered;
    }

    [[nodiscard]] std::size_t subscriptionCount() const noexcept { return subCount_; }
    [[nodiscard]] std::size_t queuedEventCount() const noexcept { return queuedCount_; }

private:
    std::array<EventSubscription, MaxSubscriptions> subscriptions_{};
    std::array<EventMessage, MaxQueuedEvents> queued_{};
    std::size_t subCount_{};
    std::size_t queuedCount_{};
};

} // namespace marble::gameplay
