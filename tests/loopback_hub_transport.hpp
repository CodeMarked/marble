#pragma once

#include "gameplay/GameTransport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace marble::gameplay::test {

/// Fan-out loopback: server peer **1**, clients **2** and **3**. For session tests only.
template <std::size_t MaxPayload, std::size_t QueueDepth>
class LoopbackHubShared {
public:
    struct Slot {
        PeerId from{kInvalidPeerId};
        std::uint16_t len{};
        std::array<std::uint8_t, MaxPayload> bytes{};
    };

    struct Queue {
        std::array<Slot, QueueDepth> slots{};
        std::size_t head{};
        std::size_t count{};

        [[nodiscard]] bool push(PeerId from, void const* data, std::size_t len) noexcept {
            if (data == nullptr || len == 0u || len > MaxPayload || count >= QueueDepth) {
                return false;
            }
            std::size_t const idx = (head + count) % QueueDepth;
            slots[idx].from = from;
            slots[idx].len = static_cast<std::uint16_t>(len);
            std::memcpy(slots[idx].bytes.data(), data, len);
            ++count;
            return true;
        }

        [[nodiscard]] std::size_t pop(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept {
            if (count == 0u || buffer == nullptr || bufferBytes == 0u) {
                return 0u;
            }
            Slot const& s = slots[head];
            if (static_cast<std::size_t>(s.len) > bufferBytes) {
                return 0u;
            }
            outFrom = s.from;
            std::memcpy(buffer, s.bytes.data(), static_cast<std::size_t>(s.len));
            head = (head + 1u) % QueueDepth;
            --count;
            return static_cast<std::size_t>(s.len);
        }
    };

    Queue toServer{};
    Queue toClient2{};
    Queue toClient3{};
};

template <std::size_t MaxPayload, std::size_t QueueDepth>
class LoopbackHubServerTransport final : public IGameTransport {
public:
    explicit LoopbackHubServerTransport(LoopbackHubShared<MaxPayload, QueueDepth>* hub) noexcept
        : hub_(hub) {}

    [[nodiscard]] bool send(PeerId to, void const* data, std::size_t len) noexcept override {
        if (hub_ == nullptr) {
            return false;
        }
        static constexpr PeerId kServer = 1u;
        if (to == 2u) {
            return hub_->toClient2.push(kServer, data, len);
        }
        if (to == 3u) {
            return hub_->toClient3.push(kServer, data, len);
        }
        return false;
    }

    [[nodiscard]] std::size_t receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept override {
        if (hub_ == nullptr) {
            return 0u;
        }
        return hub_->toServer.pop(outFrom, buffer, bufferBytes);
    }

    void forgetPeer(PeerId /*peer*/) noexcept override {}

private:
    LoopbackHubShared<MaxPayload, QueueDepth>* hub_{};
};

template <std::size_t MaxPayload, std::size_t QueueDepth>
class LoopbackHubClientTransport final : public IGameTransport {
public:
    LoopbackHubClientTransport(
        LoopbackHubShared<MaxPayload, QueueDepth>* hub,
        PeerId selfId,
        typename LoopbackHubShared<MaxPayload, QueueDepth>::Queue* inbound
    ) noexcept
        : hub_(hub)
        , self_(selfId)
        , inbound_(inbound) {}

    [[nodiscard]] bool send(PeerId /*to*/, void const* data, std::size_t len) noexcept override {
        if (hub_ == nullptr) {
            return false;
        }
        return hub_->toServer.push(self_, data, len);
    }

    [[nodiscard]] std::size_t receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept override {
        if (inbound_ == nullptr) {
            return 0u;
        }
        return inbound_->pop(outFrom, buffer, bufferBytes);
    }

    void forgetPeer(PeerId /*peer*/) noexcept override {}

private:
    LoopbackHubShared<MaxPayload, QueueDepth>* hub_{};
    PeerId self_{kInvalidPeerId};
    typename LoopbackHubShared<MaxPayload, QueueDepth>::Queue* inbound_{};
};

} // namespace marble::gameplay::test
