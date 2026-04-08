#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace marble::gameplay {

/// Sequence-number comparison with uint16_t wrap-around.
/// Returns true if `a` is logically after `b` (within half the sequence space).
[[nodiscard]] constexpr bool sequenceAfter(std::uint16_t a, std::uint16_t b) noexcept {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(a - b)) > 0;
}

/// Per-peer reliable delivery: outgoing sequence counter, incoming ack tracking,
/// and a fixed-capacity retransmit ring buffer. Allocation-free, tick-driven ([ADR-0060]).
template <std::size_t RetransmitCapacity, std::size_t MaxMessageBytes = 128>
class ReliableChannel {
public:
    static_assert(RetransmitCapacity > 0 && RetransmitCapacity <= 256,
                  "ReliableChannel requires 1..256 retransmit slots");
    static_assert(MaxMessageBytes > 0 && MaxMessageBytes <= 2048,
                  "ReliableChannel max message size must be 1..2048");

    struct RetransmitEntry {
        std::uint16_t sequence{};
        std::uint32_t sendTick{};
        std::uint16_t len{};
        std::array<std::uint8_t, MaxMessageBytes> bytes{};
        bool active{};
    };

    /// Allocate the next outgoing sequence number (1-based, skips 0 on wrap).
    [[nodiscard]] std::uint16_t nextSequence() noexcept {
        std::uint16_t const seq = nextOutSeq_;
        ++nextOutSeq_;
        if (nextOutSeq_ == 0u) {
            nextOutSeq_ = 1u;
        }
        return seq;
    }

    /// Store a fully-framed reliable message for potential retransmit.
    [[nodiscard]] bool recordOutgoing(
        std::uint16_t seq,
        void const* data,
        std::size_t len,
        std::uint32_t sendTick
    ) noexcept {
        if (data == nullptr || len == 0u || len > MaxMessageBytes) {
            return false;
        }
        RetransmitEntry* slot = findInactiveSlot();
        if (slot == nullptr) {
            slot = findOldestActive();
        }
        if (slot == nullptr) {
            return false;
        }
        slot->sequence = seq;
        slot->sendTick = sendTick;
        slot->len = static_cast<std::uint16_t>(len);
        std::memcpy(slot->bytes.data(), data, len);
        slot->active = true;
        return true;
    }

    /// Record an incoming remote sequence. Updates recv tracking for ack piggybacking.
    /// Returns false if this sequence was already seen (duplicate or too old).
    [[nodiscard]] bool processIncomingSequence(std::uint16_t remoteSeq) noexcept {
        if (!recvInitialized_) {
            lastRecvSeq_ = remoteSeq;
            recvBitmap_ = 0u;
            recvInitialized_ = true;
            return true;
        }
        if (remoteSeq == lastRecvSeq_) {
            return false;
        }
        if (sequenceAfter(remoteSeq, lastRecvSeq_)) {
            std::uint32_t const diff =
                static_cast<std::uint32_t>(static_cast<std::uint16_t>(remoteSeq - lastRecvSeq_));
            if (diff >= 32u) {
                recvBitmap_ = 0u;
            } else {
                recvBitmap_ = (recvBitmap_ << diff) | (1u << (diff - 1u));
            }
            lastRecvSeq_ = remoteSeq;
            return true;
        }
        std::uint32_t const diff =
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(lastRecvSeq_ - remoteSeq));
        if (diff > 32u) {
            return false;
        }
        std::uint32_t const bit = 1u << (diff - 1u);
        if (recvBitmap_ & bit) {
            return false;
        }
        recvBitmap_ |= bit;
        return true;
    }

    /// Process piggybacked acks from a received message. Retires confirmed retransmit entries.
    void processAcks(std::uint16_t ackSeq, std::uint32_t ackBits) noexcept {
        for (auto& e : retransmit_) {
            if (!e.active) {
                continue;
            }
            if (isAcked(e.sequence, ackSeq, ackBits)) {
                e.active = false;
            }
        }
    }

    /// Current ack sequence to piggyback on outgoing messages.
    [[nodiscard]] std::uint16_t ackSequence() const noexcept { return lastRecvSeq_; }

    /// Current ack bitmap to piggyback on outgoing messages.
    [[nodiscard]] std::uint32_t ackBitmap() const noexcept { return recvBitmap_; }

    /// Find the oldest un-acked entry whose age exceeds `rttEstimateTicks`.
    /// Returns nullptr if nothing needs retransmit.
    [[nodiscard]] RetransmitEntry* pendingRetransmit(
        std::uint32_t currentTick,
        std::uint32_t rttEstimateTicks
    ) noexcept {
        RetransmitEntry* oldest = nullptr;
        for (auto& e : retransmit_) {
            if (!e.active) {
                continue;
            }
            if (currentTick - e.sendTick < rttEstimateTicks) {
                continue;
            }
            if (oldest == nullptr || sequenceAfter(oldest->sequence, e.sequence)) {
                oldest = &e;
            }
        }
        return oldest;
    }

    /// Update the send tick of a retransmit entry (call after resending).
    void markResent(std::uint16_t seq, std::uint32_t newSendTick) noexcept {
        for (auto& e : retransmit_) {
            if (e.active && e.sequence == seq) {
                e.sendTick = newSendTick;
                return;
            }
        }
    }

    /// Number of active (un-acked) entries in the retransmit buffer.
    [[nodiscard]] std::size_t activeCount() const noexcept {
        std::size_t n = 0u;
        for (auto const& e : retransmit_) {
            if (e.active) {
                ++n;
            }
        }
        return n;
    }

    void reset() noexcept {
        nextOutSeq_ = 1u;
        lastRecvSeq_ = 0u;
        recvBitmap_ = 0u;
        recvInitialized_ = false;
        for (auto& e : retransmit_) {
            e = {};
        }
    }

private:
    [[nodiscard]] static constexpr bool isAcked(
        std::uint16_t seq,
        std::uint16_t ackSeq,
        std::uint32_t ackBits
    ) noexcept {
        if (seq == ackSeq) {
            return true;
        }
        if (!sequenceAfter(ackSeq, seq)) {
            return false;
        }
        std::uint32_t const diff =
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(ackSeq - seq));
        if (diff > 32u) {
            return false;
        }
        return (ackBits & (1u << (diff - 1u))) != 0u;
    }

    [[nodiscard]] RetransmitEntry* findInactiveSlot() noexcept {
        for (auto& e : retransmit_) {
            if (!e.active) {
                return &e;
            }
        }
        return nullptr;
    }

    [[nodiscard]] RetransmitEntry* findOldestActive() noexcept {
        RetransmitEntry* oldest = nullptr;
        for (auto& e : retransmit_) {
            if (!e.active) {
                continue;
            }
            if (oldest == nullptr || sequenceAfter(oldest->sequence, e.sequence)) {
                oldest = &e;
            }
        }
        return oldest;
    }

    std::uint16_t nextOutSeq_{1u};
    std::uint16_t lastRecvSeq_{};
    std::uint32_t recvBitmap_{};
    bool recvInitialized_{};
    std::array<RetransmitEntry, RetransmitCapacity> retransmit_{};
};

} // namespace marble::gameplay
