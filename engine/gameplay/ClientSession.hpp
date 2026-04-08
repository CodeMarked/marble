#pragma once

#include "gameplay/GameTransport.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/ReliableChannel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

/// Client-side session helper: handles handshake, reliable acks, and snapshot
/// ring buffer for future interpolation ([ADR-0054], [ADR-0060]).
///
/// Driven externally via `tick(dt)`.
template <std::size_t SnapshotRingCapacity = 32, std::size_t RetransmitCapacity = 8>
class ClientSession {
public:
    static constexpr std::size_t kMaxMessageBytes = 128;
    static constexpr std::size_t kTransportBufSize = 2048;
    static constexpr std::uint32_t kHelloRetryTicks = 30;
    static constexpr std::uint32_t kRttEstimateTicks = 6;
    static constexpr std::size_t kMaxSnapshotPayload = 1024;

    struct SnapshotEntry {
        std::uint32_t receiveTick{};
        std::uint16_t payloadLen{};
        std::array<std::uint8_t, kMaxSnapshotPayload> payload{};
        bool valid{};
    };

    [[nodiscard]] bool initialize(
        IGameTransport* transport,
        PeerId serverPeerId,
        std::uint16_t tickHz = 60u,
        std::uint32_t clientNonce = 0xdeadbeefu
    ) noexcept {
        if (transport == nullptr || serverPeerId == kInvalidPeerId) {
            return false;
        }
        transport_ = transport;
        serverPeerId_ = serverPeerId;
        tickHz_ = tickHz > 0u ? tickHz : 60u;
        clientNonce_ = clientNonce;
        state_ = ConnectionState::Disconnected;
        assignedPeerId_ = kInvalidPeerId;
        simTick_ = 0u;
        tickAccumulator_ = 0.f;
        helloSendTick_ = 0u;
        channel_.reset();
        snapshotHead_ = 0u;
        snapshotCount_ = 0u;
        for (auto& s : snapshotRing_) {
            s = {};
        }
        initialized_ = true;
        return true;
    }

    void tick(float dt) noexcept {
        if (!initialized_ || transport_ == nullptr) {
            return;
        }
        tickAccumulator_ += dt;
        float const tickDuration = 1.0f / static_cast<float>(tickHz_);
        while (tickAccumulator_ >= tickDuration) {
            tickAccumulator_ -= tickDuration;
            ++simTick_;

            if (state_ == ConnectionState::Disconnected) {
                sendHello();
                state_ = ConnectionState::Connecting;
                helloSendTick_ = simTick_;
            } else if (state_ == ConnectionState::Connecting) {
                if (simTick_ - helloSendTick_ >= kHelloRetryTicks) {
                    sendHello();
                    helloSendTick_ = simTick_;
                }
            }

            processTransport();
        }
    }

    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] PeerId assignedPeerId() const noexcept { return assignedPeerId_; }
    [[nodiscard]] std::uint32_t currentTick() const noexcept { return simTick_; }

    [[nodiscard]] std::size_t snapshotCount() const noexcept { return snapshotCount_; }

    /// Access the i-th most recent snapshot (0 = newest).
    [[nodiscard]] SnapshotEntry const* snapshotAt(std::size_t i) const noexcept {
        if (i >= snapshotCount_) {
            return nullptr;
        }
        std::size_t const idx =
            (snapshotHead_ + SnapshotRingCapacity - 1u - i) % SnapshotRingCapacity;
        return &snapshotRing_[idx];
    }

    /// Parse entity snapshots from the latest ring buffer entry.
    [[nodiscard]] std::size_t readLatestEntities(
        EntityKinematicsSnapshot* out,
        std::size_t maxCount
    ) const noexcept {
        if (snapshotCount_ == 0u || out == nullptr || maxCount == 0u) {
            return 0u;
        }
        SnapshotEntry const* entry = snapshotAt(0);
        if (entry == nullptr || !entry->valid) {
            return 0u;
        }
        std::size_t count = 0u;
        std::size_t offset = 0u;
        while (offset + kEntityKinematicsSnapshotWireBytes <= entry->payloadLen && count < maxCount) {
            if (!readEntityKinematicsSnapshot(entry->payload.data() + offset,
                                              entry->payloadLen - offset, out[count])) {
                break;
            }
            offset += kEntityKinematicsSnapshotWireBytes;
            ++count;
        }
        return count;
    }

    void disconnect() noexcept {
        if (state_ != ConnectionState::Connected || transport_ == nullptr) {
            return;
        }
        std::uint16_t const seq = channel_.nextSequence();
        std::array<std::uint8_t, kMaxMessageBytes> buf{};
        std::size_t const len = writeSessionDisconnect(
            buf.data(), buf.size(), seq,
            channel_.ackSequence(), channel_.ackBitmap()
        );
        if (len > 0u) {
            static_cast<void>(channel_.recordOutgoing(seq, buf.data(), len, simTick_));
            static_cast<void>(transport_->send(serverPeerId_, buf.data(), len));
        }
        state_ = ConnectionState::Disconnected;
    }

private:
    void sendHello() noexcept {
        SessionHelloPayload hello{};
        hello.clientUdpPortHost = 0u;
        hello.clientNonce = clientNonce_;
        std::array<std::uint8_t, 64> buf{};
        std::size_t const len = writeSessionHello(buf.data(), buf.size(), hello);
        if (len > 0u) {
            static_cast<void>(transport_->send(serverPeerId_, buf.data(), len));
        }
    }

    void processTransport() noexcept {
        std::array<std::uint8_t, kTransportBufSize> buf{};
        PeerId from{kInvalidPeerId};

        for (;;) {
            std::size_t const n = transport_->receive(from, buf.data(), buf.size());
            if (n == 0u) {
                break;
            }
            if (from != serverPeerId_) {
                continue;
            }

            SessionMessageType type{};
            std::uint8_t flags{};
            std::uint8_t const* payload{};
            std::size_t payloadLen{};
            if (!parseSessionEnvelopeEx(buf.data(), n, type, flags, payload, payloadLen)) {
                continue;
            }

            bool needsAckResponse = false;
            if (flags & kSessionEnvelopeFlag_Reliable) {
                std::uint16_t seq{};
                std::uint16_t ackSeq{};
                std::uint32_t ackBits{};
                if (!readReliableHeader(payload, payloadLen, seq, ackSeq, ackBits)) {
                    continue;
                }
                payload += kReliableHeaderBytes;
                payloadLen -= kReliableHeaderBytes;

                static_cast<void>(channel_.processIncomingSequence(seq));
                channel_.processAcks(ackSeq, ackBits);
                needsAckResponse = true;
            }

            switch (type) {
            case SessionMessageType::HelloAck: {
                SessionHelloAckPayload ack{};
                if (!readSessionHelloAck(payload, payloadLen, ack)) {
                    break;
                }
                if (ack.echoClientNonce != clientNonce_) {
                    break;
                }
                assignedPeerId_ = ack.assignedPeerId;
                state_ = ConnectionState::Connected;
                break;
            }
            case SessionMessageType::GameSnapshot:
                if (state_ == ConnectionState::Connected) {
                    storeSnapshot(payload, payloadLen);
                }
                break;
            case SessionMessageType::Ack:
                break;
            case SessionMessageType::Disconnect:
                state_ = ConnectionState::Disconnected;
                break;
            case SessionMessageType::Hello:
                break;
            }

            if (needsAckResponse) {
                sendAck();
            }
        }
    }

    void sendAck() noexcept {
        std::uint16_t const seq = channel_.nextSequence();
        std::array<std::uint8_t, kMaxMessageBytes> buf{};
        std::size_t const len = writeSessionAck(
            buf.data(), buf.size(), seq,
            channel_.ackSequence(), channel_.ackBitmap()
        );
        if (len > 0u) {
            static_cast<void>(transport_->send(serverPeerId_, buf.data(), len));
        }
    }

    void storeSnapshot(std::uint8_t const* data, std::size_t len) noexcept {
        if (data == nullptr || len == 0u || len > kMaxSnapshotPayload) {
            return;
        }
        SnapshotEntry& entry = snapshotRing_[snapshotHead_];
        entry.receiveTick = simTick_;
        entry.payloadLen = static_cast<std::uint16_t>(len);
        std::memcpy(entry.payload.data(), data, len);
        entry.valid = true;
        snapshotHead_ = (snapshotHead_ + 1u) % SnapshotRingCapacity;
        if (snapshotCount_ < SnapshotRingCapacity) {
            ++snapshotCount_;
        }
    }

    IGameTransport* transport_{};
    PeerId serverPeerId_{kInvalidPeerId};
    PeerId assignedPeerId_{kInvalidPeerId};
    std::uint16_t tickHz_{60u};
    std::uint32_t clientNonce_{};
    ConnectionState state_{ConnectionState::Disconnected};
    bool initialized_{};

    std::uint32_t simTick_{};
    float tickAccumulator_{};
    std::uint32_t helloSendTick_{};

    ReliableChannel<RetransmitCapacity, kMaxMessageBytes> channel_{};
    std::array<SnapshotEntry, SnapshotRingCapacity> snapshotRing_{};
    std::size_t snapshotHead_{};
    std::size_t snapshotCount_{};
};

} // namespace marble::gameplay
