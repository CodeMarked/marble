#pragma once

#include "gameplay/GameTransport.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/ReliableChannel.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

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
    /// Simulated ticks between post-connect [`SessionMessageType::TimePing`] sends (protocol v4).
    static constexpr std::uint32_t kTimePingIntervalTicks = 30u;
    /// Drop outstanding ping if no [`SessionMessageType::TimePong`] by this many client sim ticks.
    static constexpr std::uint32_t kTimePingTimeoutTicks = 120u;

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
        std::uint32_t clientNonce = 0xdeadbeefu,
        std::uint32_t joinTokenU32 = 0u
    ) noexcept {
        if (transport == nullptr || serverPeerId == kInvalidPeerId) {
            return false;
        }
        transport_ = transport;
        serverPeerId_ = serverPeerId;
        tickHz_ = tickHz > 0u ? tickHz : 60u;
        clientNonce_ = clientNonce;
        helloJoinTokenU32_ = joinTokenU32;
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
        resetServerTimeSync_();
        outstandingPingId_ = 0u;
        ticksSinceLastPing_ = 0u;
        pingSendCounter_ = 1u;
        pendingCorrection_.reset();
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

            maybeAdvanceTimePing_();
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

    /// Refine server tick mapping when an authoritative snapshot arrives ([ADR-0060] presentation timeline).
    void noteAuthoritativeSnapshot(std::uint32_t serverSimTick) noexcept {
        lastSyncServerSimTick_ = serverSimTick;
        lastSyncClientSteady_ = std::chrono::steady_clock::now();
        hasServerTimeSync_ = true;
    }

    [[nodiscard]] bool hasServerTimeSync() const noexcept { return hasServerTimeSync_; }

    /// Exponential moving average of RTT (seconds): Hello→HelloAck, then refined by TimePing→TimePong.
    /// Zero if not yet measured.
    [[nodiscard]] float estimatedRttSeconds() const noexcept { return rttEmaSeconds_; }

    /// Reliable [`SessionMessageType::StateCorrection`] from server (optional vertical-slice hook).
    [[nodiscard]] bool takePendingStateCorrection(SessionStateCorrectionPayload& out) noexcept {
        if (!pendingCorrection_.has_value()) {
            return false;
        }
        out = *pendingCorrection_;
        pendingCorrection_.reset();
        return true;
    }

    /// Estimated server `simTick` at `steady_clock::now()` from last sync + `simulationHz`.
    [[nodiscard]] float estimatedServerSimTickAtNow(float simulationHz) const noexcept {
        if (!hasServerTimeSync_ || simulationHz <= 0.f) {
            return 0.f;
        }
        auto const now = std::chrono::steady_clock::now();
        float const sec = std::chrono::duration<float>(now - lastSyncClientSteady_).count();
        return static_cast<float>(lastSyncServerSimTick_) + sec * simulationHz;
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
        pendingCorrection_.reset();
        resetServerTimeSync_();
    }

private:
    void resetServerTimeSync_() noexcept {
        hasServerTimeSync_ = false;
        lastSyncServerSimTick_ = 0u;
        lastSyncClientSteady_ = {};
        helloSendSteadyValid_ = false;
        rttEmaSeconds_ = 0.f;
        hasRttEstimate_ = false;
        outstandingPingId_ = 0u;
        ticksSinceLastPing_ = 0u;
        pingSendCounter_ = 1u;
    }

    void applyRttSample_(float rttSec) noexcept {
        if (rttSec < 0.f) {
            return;
        }
        if (hasRttEstimate_) {
            rttEmaSeconds_ = rttEmaSeconds_ * 0.875f + rttSec * 0.125f;
        } else {
            rttEmaSeconds_ = rttSec;
            hasRttEstimate_ = true;
        }
    }

    void maybeAdvanceTimePing_() noexcept {
        if (state_ != ConnectionState::Connected || transport_ == nullptr) {
            return;
        }
        if (outstandingPingId_ != 0u) {
            if (simTick_ - pingSendSimTick_ >= kTimePingTimeoutTicks) {
                outstandingPingId_ = 0u;
            } else {
                return;
            }
        }
        ++ticksSinceLastPing_;
        if (ticksSinceLastPing_ >= kTimePingIntervalTicks) {
            sendTimePing_();
            ticksSinceLastPing_ = 0u;
        }
    }

    void sendTimePing_() noexcept {
        std::uint32_t const id = pingSendCounter_;
        ++pingSendCounter_;
        if (pingSendCounter_ == 0u) {
            pingSendCounter_ = 1u;
        }
        if (id == 0u) {
            return;
        }
        SessionTimePingPayload ping{};
        ping.clientPingId = id;

        std::uint16_t const seq = channel_.nextSequence();
        std::array<std::uint8_t, kMaxMessageBytes> buf{};
        std::size_t const len = writeSessionTimePingReliable(
            buf.data(), buf.size(), ping,
            seq, channel_.ackSequence(), channel_.ackBitmap()
        );
        if (len == 0u) {
            return;
        }
        if (!channel_.recordOutgoing(seq, buf.data(), len, simTick_)) {
            return;
        }
        static_cast<void>(transport_->send(serverPeerId_, buf.data(), len));
        outstandingPingId_ = id;
        pingSendSteady_ = std::chrono::steady_clock::now();
        pingSendSimTick_ = simTick_;
    }

    void sendHello() noexcept {
        SessionHelloPayload hello{};
        hello.clientUdpPortHost = 0u;
        hello.clientNonce = clientNonce_;
        hello.joinTokenU32 = helloJoinTokenU32_;
        std::array<std::uint8_t, 64> buf{};
        std::size_t const len = writeSessionHello(buf.data(), buf.size(), hello);
        if (len > 0u) {
            helloSendSteady_ = std::chrono::steady_clock::now();
            helloSendSteadyValid_ = true;
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
                auto const ackRecvTime = std::chrono::steady_clock::now();
                if (helloSendSteadyValid_) {
                    float const rtt = std::chrono::duration<float>(ackRecvTime - helloSendSteady_).count();
                    applyRttSample_(rtt);
                }
                lastSyncServerSimTick_ = ack.serverSimTickAtAck;
                lastSyncClientSteady_ = ackRecvTime;
                hasServerTimeSync_ = true;
                state_ = ConnectionState::Connected;
                break;
            }
            case SessionMessageType::GameSnapshot:
                if (state_ == ConnectionState::Connected) {
                    storeSnapshot(payload, payloadLen);
                }
                break;
            case SessionMessageType::TimePong: {
                if (state_ != ConnectionState::Connected) {
                    break;
                }
                SessionTimePongPayload pong{};
                if (!readSessionTimePong(payload, payloadLen, pong)) {
                    break;
                }
                if (outstandingPingId_ == 0u || pong.clientPingId != outstandingPingId_) {
                    break;
                }
                auto const recvTime = std::chrono::steady_clock::now();
                float const rtt = std::chrono::duration<float>(recvTime - pingSendSteady_).count();
                applyRttSample_(rtt);
                lastSyncServerSimTick_ = pong.serverSimTick;
                lastSyncClientSteady_ = recvTime;
                hasServerTimeSync_ = true;
                outstandingPingId_ = 0u;
                break;
            }
            case SessionMessageType::Ack:
                break;
            case SessionMessageType::Disconnect:
                state_ = ConnectionState::Disconnected;
                resetServerTimeSync_();
                break;
            case SessionMessageType::Hello:
                break;
            case SessionMessageType::ClientInput:
                break;
            case SessionMessageType::TimePing:
                break;
            case SessionMessageType::StateCorrection:
                if (state_ == ConnectionState::Connected && (flags & kSessionEnvelopeFlag_Reliable) != 0) {
                    SessionStateCorrectionPayload corr{};
                    if (readSessionStateCorrection(payload, payloadLen, corr)) {
                        pendingCorrection_ = corr;
                    }
                }
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
    std::uint32_t helloJoinTokenU32_{};
    ConnectionState state_{ConnectionState::Disconnected};
    bool initialized_{};

    std::uint32_t simTick_{};
    float tickAccumulator_{};
    std::uint32_t helloSendTick_{};

    ReliableChannel<RetransmitCapacity, kMaxMessageBytes> channel_{};
    std::array<SnapshotEntry, SnapshotRingCapacity> snapshotRing_{};
    std::size_t snapshotHead_{};
    std::size_t snapshotCount_{};

    std::chrono::steady_clock::time_point lastSyncClientSteady_{};
    std::chrono::steady_clock::time_point helloSendSteady_{};
    std::uint32_t lastSyncServerSimTick_{};
    float rttEmaSeconds_{};
    bool hasServerTimeSync_{};
    bool helloSendSteadyValid_{};
    bool hasRttEstimate_{};

    std::uint32_t outstandingPingId_{};
    std::uint32_t ticksSinceLastPing_{};
    std::uint32_t pingSendCounter_{1u};
    std::uint32_t pingSendSimTick_{};
    std::chrono::steady_clock::time_point pingSendSteady_{};

    std::optional<SessionStateCorrectionPayload> pendingCorrection_{};
};

} // namespace marble::gameplay
