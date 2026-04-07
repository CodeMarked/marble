#pragma once

#include "gameplay/GameTransport.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/ReliableChannel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

/// Small replicated entity state driven by external simulation and emitted as snapshots.
struct ReplicatedEntity {
    WorldObjectRef entity{};
    math::Vec3 position{};
    math::Vec3 velocity{};
    PhysicsSimulationTier tier{PhysicsSimulationTier::Contact};
    bool active{};
};

/// Authoritative session manager: fixed-tick loop driving small replicated state
/// with per-peer reliable channels for control messages ([ADR-0054], [ADR-0060]).
///
/// Driven externally via `tick(dt)` — does NOT own the loop; usable from
/// `FixedStepSimulationPhase::step()` or a standalone `while(running)`.
template <std::size_t MaxPlayers = 8, std::size_t RetransmitCapacity = 16>
class AuthoritativeSession {
public:
    static constexpr std::size_t kMaxEntities = 16;
    static constexpr std::size_t kMaxMessageBytes = 128;
    static constexpr std::uint32_t kRttEstimateTicks = 6;
    static constexpr std::size_t kTransportBufSize = 2048;

    struct PeerState {
        PeerId peer{kInvalidPeerId};
        ConnectionState connectionState{ConnectionState::Disconnected};
        ReliableChannel<RetransmitCapacity, kMaxMessageBytes> channel{};
        std::uint32_t lastNonce{};
        bool active{};
    };

    [[nodiscard]] bool initialize(SessionConfig const& config, IGameTransport* transport) noexcept {
        if (!isValid(config) || transport == nullptr) {
            return false;
        }
        if (config.mode == MultiplayerMode::Offline) {
            return false;
        }
        config_ = config;
        transport_ = transport;
        simTick_ = 0u;
        tickAccumulator_ = 0.f;
        ticksSinceSnapshot_ = 0u;
        nextPeerId_ = 2u;
        for (auto& ps : peerStates_) {
            ps = {};
        }
        for (auto& e : entities_) {
            e = {};
        }
        roster_.clear();
        if (!roster_.bootstrap(config_.mode)) {
            return false;
        }
        initialized_ = true;
        return true;
    }

    void tick(float dt) noexcept {
        if (!initialized_ || transport_ == nullptr) {
            return;
        }
        tickAccumulator_ += dt;
        float const tickDuration = 1.0f / static_cast<float>(config_.simulationHz);
        while (tickAccumulator_ >= tickDuration) {
            tickAccumulator_ -= tickDuration;
            ++simTick_;
            processTransport();
            ++ticksSinceSnapshot_;
            std::uint32_t const ticksPerSnapshot =
                config_.simulationHz / config_.snapshotHz;
            if (ticksSinceSnapshot_ >= ticksPerSnapshot) {
                emitSnapshots();
                ticksSinceSnapshot_ = 0u;
            }
            processRetransmits();
        }
    }

    [[nodiscard]] std::uint32_t currentTick() const noexcept { return simTick_; }
    [[nodiscard]] SessionConfig const& sessionConfig() const noexcept { return config_; }
    [[nodiscard]] AuthorityRoster<MaxPlayers> const& roster() const noexcept { return roster_; }

    [[nodiscard]] std::size_t peerCount() const noexcept {
        std::size_t n = 0u;
        for (auto const& ps : peerStates_) {
            if (ps.active && ps.connectionState == ConnectionState::Connected) {
                ++n;
            }
        }
        return n;
    }

    [[nodiscard]] bool isConnected(PeerId peer) const noexcept {
        PeerState const* ps = findPeerState(peer);
        return ps != nullptr && ps->connectionState == ConnectionState::Connected;
    }

    [[nodiscard]] ReplicatedEntity* entities() noexcept { return entities_.data(); }
    [[nodiscard]] ReplicatedEntity const* entities() const noexcept { return entities_.data(); }
    [[nodiscard]] static constexpr std::size_t maxEntities() noexcept { return kMaxEntities; }

    [[nodiscard]] bool setEntity(
        std::size_t index,
        WorldObjectRef ref,
        math::Vec3 pos,
        math::Vec3 vel,
        PhysicsSimulationTier t = PhysicsSimulationTier::Contact
    ) noexcept {
        if (index >= kMaxEntities) {
            return false;
        }
        entities_[index] = ReplicatedEntity{ref, pos, vel, t, true};
        return true;
    }

    void clearEntity(std::size_t index) noexcept {
        if (index < kMaxEntities) {
            entities_[index] = {};
        }
    }

    [[nodiscard]] PeerState const* peerStateFor(PeerId peer) const noexcept {
        return findPeerState(peer);
    }

private:
    void processTransport() noexcept {
        std::array<std::uint8_t, kTransportBufSize> buf{};
        PeerId from{kInvalidPeerId};

        for (;;) {
            std::size_t const n = transport_->receive(from, buf.data(), buf.size());
            if (n == 0u) {
                break;
            }

            SessionMessageType type{};
            std::uint8_t flags{};
            std::uint8_t const* payload{};
            std::size_t payloadLen{};
            if (!parseSessionEnvelopeEx(buf.data(), n, type, flags, payload, payloadLen)) {
                continue;
            }

            PeerState* ps = findPeerState(from);

            if (flags & kSessionEnvelopeFlag_Reliable) {
                std::uint16_t seq{};
                std::uint16_t ackSeq{};
                std::uint32_t ackBits{};
                if (!readReliableHeader(payload, payloadLen, seq, ackSeq, ackBits)) {
                    continue;
                }
                payload += kReliableHeaderBytes;
                payloadLen -= kReliableHeaderBytes;

                if (ps != nullptr) {
                    static_cast<void>(ps->channel.processIncomingSequence(seq));
                    ps->channel.processAcks(ackSeq, ackBits);
                }
            }

            switch (type) {
            case SessionMessageType::Hello: {
                SessionHelloPayload hello{};
                if (!readSessionHello(payload, payloadLen, hello)) {
                    break;
                }
                handleHello(from, hello);
                break;
            }
            case SessionMessageType::Ack:
                break;
            case SessionMessageType::Disconnect:
                if (ps != nullptr) {
                    handleDisconnect(from, ps);
                }
                break;
            case SessionMessageType::HelloAck:
            case SessionMessageType::GameSnapshot:
                break;
            }
        }
    }

    void handleHello(PeerId from, SessionHelloPayload const& hello) noexcept {
        PeerState* ps = findPeerState(from);
        if (ps != nullptr) {
            sendHelloAckReliable(ps, from, hello.clientNonce);
            return;
        }

        ps = allocatePeerState(from);
        if (ps == nullptr) {
            return;
        }

        if (!roster_.connectPeer(from)) {
            ps->active = false;
            return;
        }
        if (!roster_.transition(from, ConnectionState::Handshaking)) {
            ps->active = false;
            return;
        }

        sendHelloAckReliable(ps, from, hello.clientNonce);

        static_cast<void>(roster_.transition(from, ConnectionState::Connected));
        ps->connectionState = ConnectionState::Connected;
    }

    void handleDisconnect(PeerId from, PeerState* ps) noexcept {
        if (ps->connectionState == ConnectionState::Connected) {
            static_cast<void>(roster_.transition(from, ConnectionState::TimingOut));
            static_cast<void>(roster_.transition(from, ConnectionState::Disconnected));
        } else {
            static_cast<void>(roster_.transition(from, ConnectionState::Disconnected));
        }
        ps->active = false;
        ps->channel.reset();
        ps->connectionState = ConnectionState::Disconnected;
    }

    void sendHelloAckReliable(PeerState* ps, PeerId to, std::uint32_t clientNonce) noexcept {
        SessionHelloAckPayload ack{};
        ack.assignedPeerId = to;
        ack.echoClientNonce = clientNonce;

        std::uint16_t const seq = ps->channel.nextSequence();
        std::array<std::uint8_t, kMaxMessageBytes> buf{};
        std::size_t const len = writeSessionHelloAckReliable(
            buf.data(), buf.size(), ack,
            seq, ps->channel.ackSequence(), ps->channel.ackBitmap()
        );
        if (len == 0u) {
            return;
        }
        static_cast<void>(ps->channel.recordOutgoing(seq, buf.data(), len, simTick_));
        ps->lastNonce = clientNonce;
        static_cast<void>(transport_->send(to, buf.data(), len));
    }

    void emitSnapshots() noexcept {
        std::array<std::uint8_t, kTransportBufSize> inner{};
        std::size_t offset = 0u;

        for (auto const& ent : entities_) {
            if (!ent.active) {
                continue;
            }
            EntityKinematicsSnapshot snap{};
            snap.simTick = simTick_;
            snap.entity = ent.entity;
            snap.tier = ent.tier;
            snap.positionLocal = ent.position;
            snap.linearVelocity = ent.velocity;
            std::size_t const written =
                writeEntityKinematicsSnapshot(inner.data() + offset, inner.size() - offset, snap);
            if (written == 0u) {
                break;
            }
            offset += written;
        }
        if (offset == 0u) {
            return;
        }

        std::array<std::uint8_t, kTransportBufSize> frame{};
        std::size_t const frameLen =
            writeSessionGameSnapshot(frame.data(), frame.size(), inner.data(), offset);
        if (frameLen == 0u) {
            return;
        }

        for (auto const& ps : peerStates_) {
            if (!ps.active || ps.connectionState != ConnectionState::Connected) {
                continue;
            }
            static_cast<void>(transport_->send(ps.peer, frame.data(), frameLen));
        }
    }

    void processRetransmits() noexcept {
        for (auto& ps : peerStates_) {
            if (!ps.active || ps.connectionState == ConnectionState::Disconnected) {
                continue;
            }
            auto* entry = ps.channel.pendingRetransmit(simTick_, kRttEstimateTicks);
            if (entry == nullptr) {
                continue;
            }
            // Update ack fields in the stored message to reflect latest state.
            // Reliable header starts at kSessionEnvelopeBytes.
            writeU16Le(entry->bytes.data() + kSessionEnvelopeBytes + 2u,
                       ps.channel.ackSequence());
            writeU32Le(entry->bytes.data() + kSessionEnvelopeBytes + 4u,
                       ps.channel.ackBitmap());
            static_cast<void>(transport_->send(ps.peer, entry->bytes.data(), entry->len));
            entry->sendTick = simTick_;
        }
    }

    [[nodiscard]] PeerState* findPeerState(PeerId peer) noexcept {
        for (auto& ps : peerStates_) {
            if (ps.active && ps.peer == peer) {
                return &ps;
            }
        }
        return nullptr;
    }

    [[nodiscard]] PeerState const* findPeerState(PeerId peer) const noexcept {
        for (auto const& ps : peerStates_) {
            if (ps.active && ps.peer == peer) {
                return &ps;
            }
        }
        return nullptr;
    }

    [[nodiscard]] PeerState* allocatePeerState(PeerId peer) noexcept {
        for (auto& ps : peerStates_) {
            if (!ps.active) {
                ps = {};
                ps.peer = peer;
                ps.active = true;
                ps.connectionState = ConnectionState::Connecting;
                return &ps;
            }
        }
        return nullptr;
    }

    SessionConfig config_{};
    AuthorityRoster<MaxPlayers> roster_{};
    IGameTransport* transport_{};
    bool initialized_{};

    std::uint32_t simTick_{};
    float tickAccumulator_{};
    std::uint32_t ticksSinceSnapshot_{};
    PeerId nextPeerId_{2u};

    std::array<PeerState, MaxPlayers> peerStates_{};
    std::array<ReplicatedEntity, kMaxEntities> entities_{};
};

} // namespace marble::gameplay
