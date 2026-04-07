#pragma once

#include "gameplay/OnlineMultiplayerFoundation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace marble::gameplay {

/// Thin real-time transport seam ([ADR-0060]); loopback pair for tests, [`UdpGameTransport`](UdpGameTransport.hpp) for UDP/IPv4.
class IGameTransport {
public:
    virtual ~IGameTransport() = default;

    /// Send a datagram to `to` (must be a connected peer for a given implementation).
    [[nodiscard]] virtual bool send(PeerId to, void const* data, std::size_t len) noexcept = 0;

    /// Pop one inbound datagram. Returns 0 if none. On success, sets `outFrom` and copies payload into `buffer`.
    [[nodiscard]] virtual std::size_t receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept = 0;

    /// Release routing for a peer (e.g. after disconnect). Default: no-op (loopback and tests).
    virtual void forgetPeer(PeerId peer) noexcept { (void)peer; }
};

/// Fixed-depth loopback queues between exactly two peers (tests and local simulation).
template <std::size_t MaxPayload, std::size_t QueueDepth>
struct LoopbackTransportShared {
    static_assert(MaxPayload > 0 && MaxPayload <= 65535, "MaxPayload must fit uint16_t length");
    static_assert(QueueDepth > 0, "QueueDepth required");

    struct Entry {
        std::uint16_t len{};
        std::array<std::uint8_t, MaxPayload> bytes{};
    };

    struct Queue {
        std::array<Entry, QueueDepth> slots{};
        std::size_t head{};
        std::size_t count{};
    };

    PeerId idA{kInvalidPeerId};
    PeerId idB{kInvalidPeerId};
    Queue toA{};
    Queue toB{};

    constexpr LoopbackTransportShared() noexcept = default;

    constexpr LoopbackTransportShared(PeerId a, PeerId b) noexcept
        : idA(a)
        , idB(b) {}
};

template <std::size_t MaxPayload, std::size_t QueueDepth>
class LoopbackGameTransport final : public IGameTransport {
public:
    LoopbackGameTransport() noexcept = default;

    LoopbackGameTransport(LoopbackTransportShared<MaxPayload, QueueDepth>* shared, PeerId self, PeerId peer) noexcept
        : shared_(shared)
        , self_(self)
        , peer_(peer) {}

    [[nodiscard]] bool send(PeerId to, void const* data, std::size_t len) noexcept override {
        if (shared_ == nullptr || to != peer_ || data == nullptr || len == 0u || len > MaxPayload) {
            return false;
        }
        typename LoopbackTransportShared<MaxPayload, QueueDepth>::Queue* dest = nullptr;
        if (self_ == shared_->idA && to == shared_->idB) {
            dest = &shared_->toB;
        } else if (self_ == shared_->idB && to == shared_->idA) {
            dest = &shared_->toA;
        } else {
            return false;
        }
        if (dest->count >= QueueDepth) {
            return false;
        }
        std::size_t const idx = (dest->head + dest->count) % QueueDepth;
        dest->slots[idx].len = static_cast<std::uint16_t>(len);
        std::memcpy(dest->slots[idx].bytes.data(), data, len);
        ++dest->count;
        return true;
    }

    [[nodiscard]] std::size_t receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept override {
        if (shared_ == nullptr || buffer == nullptr) {
            return 0u;
        }
        typename LoopbackTransportShared<MaxPayload, QueueDepth>::Queue* src = nullptr;
        if (self_ == shared_->idA) {
            src = &shared_->toA;
        } else if (self_ == shared_->idB) {
            src = &shared_->toB;
        } else {
            return 0u;
        }
        if (src->count == 0u) {
            return 0u;
        }
        auto const& entry = src->slots[src->head];
        std::size_t const n = entry.len;
        if (bufferBytes < n) {
            return 0u;
        }
        std::memcpy(buffer, entry.bytes.data(), n);
        src->head = (src->head + 1u) % QueueDepth;
        --src->count;
        outFrom = peer_;
        return n;
    }

private:
    LoopbackTransportShared<MaxPayload, QueueDepth>* shared_{};
    PeerId self_{kInvalidPeerId};
    PeerId peer_{kInvalidPeerId};
};

template <std::size_t MaxPayload, std::size_t QueueDepth>
struct LoopbackTransportPair {
    LoopbackTransportShared<MaxPayload, QueueDepth> shared;
    LoopbackGameTransport<MaxPayload, QueueDepth> a;
    LoopbackGameTransport<MaxPayload, QueueDepth> b;

    LoopbackTransportPair(PeerId idA, PeerId idB) noexcept
        : shared(idA, idB)
        , a(&shared, idA, idB)
        , b(&shared, idB, idA) {}
};

} // namespace marble::gameplay
