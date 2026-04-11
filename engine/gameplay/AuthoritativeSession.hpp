#pragma once

#include "gameplay/GameTransport.hpp"
#include "gameplay/InterestManagement.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/ReliableChannel.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

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
        peerLastFingerprint_.fill({});
        peerEntityEverSent_.fill({});
        peerBadPayloadCount_.fill(0);
        peerClientInputPacketsThisTick_.fill(0);
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
            peerClientInputPacketsThisTick_.fill(0);
            processTransport();
            ++ticksSinceSnapshot_;
            std::uint32_t const ticksPerSnapshot =
                config_.simulationHz / config_.snapshotHz;
            if (ticksSinceSnapshot_ >= ticksPerSnapshot) {
                emitSnapshots();
                ticksSinceSnapshot_ = 0u;
            }
            processRetransmits();
            syntheticListenHostInputPacketsQueued_ = 0;
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
        PhysicsSimulationTier t = PhysicsSimulationTier::Contact,
        float yawRadians = 0.f
    ) noexcept {
        if (index >= kMaxEntities) {
            return false;
        }
        entities_[index] = ReplicatedEntity{ref, pos, vel, yawRadians, t, true};
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

    /// Latest client input received for a given peer (zero if none received yet).
    /// The caller should consume and clear each tick to avoid re-applying stale input.
    [[nodiscard]] ClientInputWirePayload const* latestInput(PeerId peer) const noexcept {
        for (auto const& pi : peerInputs_) {
            if (pi.peer == peer && pi.hasInput) {
                return &pi.input;
            }
        }
        return nullptr;
    }

    void clearInput(PeerId peer) noexcept {
        for (auto& pi : peerInputs_) {
            if (pi.peer == peer) {
                pi.hasInput = false;
                pi.input = {};
                return;
            }
        }
    }

    /// Listen-server only: queue host input as if from `peer` (use **2** for marble slot 0; UDP joiners start at 3).
    /// Call **before** [`tick`] for that simulation step. Same per-step cap as network [`handleClientInput`].
    void submitSyntheticClientInput(PeerId peer, ClientInputWirePayload const& inp) noexcept {
        if (!initialized_ || config_.mode != MultiplayerMode::ListenServer || peer != 2u) {
            return;
        }
        if (syntheticListenHostInputPacketsQueued_ >= 4u) {
            return;
        }
        ++syntheticListenHostInputPacketsQueued_;
        for (auto& pi : peerInputs_) {
            if (pi.peer == peer) {
                pi.input = inp;
                pi.hasInput = true;
                return;
            }
        }
        for (auto& pi : peerInputs_) {
            if (pi.peer == kInvalidPeerId) {
                pi.peer = peer;
                pi.input = inp;
                pi.hasInput = true;
                return;
            }
        }
    }

    /// Enable per-peer AOI filtering. When enabled, `emitSnapshots` only sends entities
    /// within the peer's interest region. The view position is derived from the entity
    /// at `viewEntityIndex`.
    void setAoiEnabled(bool enabled) noexcept {
        if (aoiEnabled_ != enabled) {
            peerLastFingerprint_.fill({});
            peerEntityEverSent_.fill({});
        }
        aoiEnabled_ = enabled;
    }
    [[nodiscard]] bool aoiEnabled() const noexcept { return aoiEnabled_; }

    void setDefaultAoiRadius(float radius) noexcept { defaultAoiRadius_ = radius; }
    [[nodiscard]] float defaultAoiRadius() const noexcept { return defaultAoiRadius_; }

    /// Expands effective inclusion radius for **each** entity by `|velocity| * seconds` (see [`InterestRegion`]).
    void setAoiEntityVelocityLookaheadSeconds(float seconds) noexcept {
        aoiEntityVelocityLookaheadSeconds_ = seconds >= 0.f ? seconds : 0.f;
    }
    [[nodiscard]] float aoiEntityVelocityLookaheadSeconds() const noexcept {
        return aoiEntityVelocityLookaheadSeconds_;
    }

    /// Shifts AOI **center** along the view entity's velocity before distance tests (0 = off).
    /// Independent of entity-side lookahead above.
    void setAoiViewerPositionLookaheadSeconds(float seconds) noexcept {
        aoiViewerPositionLookaheadSeconds_ = seconds >= 0.f ? seconds : 0.f;
    }
    [[nodiscard]] float aoiViewerPositionLookaheadSeconds() const noexcept {
        return aoiViewerPositionLookaheadSeconds_;
    }

    /// Set which entity a peer "sees from" for AOI center. Defaults to 0.
    void setPeerViewEntity(PeerId peer, std::size_t entityIndex) noexcept {
        for (auto& pi : peerInterest_) {
            if (pi.peer == peer) {
                pi.viewEntityIndex = entityIndex;
                return;
            }
        }
        for (auto& pi : peerInterest_) {
            if (pi.peer == kInvalidPeerId) {
                pi.peer = peer;
                pi.viewEntityIndex = entityIndex;
                return;
            }
        }
    }

private:
    struct PeerInterestEntry {
        PeerId peer{kInvalidPeerId};
        std::size_t viewEntityIndex{};
    };

    void clearPeerInterest(PeerId peer) noexcept {
        for (auto& pi : peerInterest_) {
            if (pi.peer == peer) {
                pi = {};
                return;
            }
        }
    }

    [[nodiscard]] std::size_t peerStateSlot(PeerId peer) const noexcept {
        for (std::size_t i = 0u; i < peerStates_.size(); ++i) {
            if (peerStates_[i].active && peerStates_[i].peer == peer) {
                return i;
            }
        }
        return MaxPlayers;
    }

    void clearPeerReplication(std::size_t peerSlot) noexcept {
        if (peerSlot >= MaxPlayers) {
            return;
        }
        peerLastFingerprint_[peerSlot] = {};
        peerEntityEverSent_[peerSlot] = {};
        peerBadPayloadCount_[peerSlot] = 0;
    }

    void noteMalformedEnvelope(PeerId from) noexcept {
        std::size_t const slot = peerStateSlot(from);
        if (slot >= MaxPlayers) {
            return;
        }
        ++peerBadPayloadCount_[slot];
        if (peerBadPayloadCount_[slot] >= 64u) {
            PeerState* ps = findPeerState(from);
            if (ps != nullptr) {
                handleDisconnect(from, ps);
            }
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

            SessionMessageType type{};
            std::uint8_t flags{};
            std::uint8_t const* payload{};
            std::size_t payloadLen{};
            if (!parseSessionEnvelopeEx(buf.data(), n, type, flags, payload, payloadLen)) {
                noteMalformedEnvelope(from);
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
            case SessionMessageType::ClientInput:
                if (ps != nullptr && ps->connectionState == ConnectionState::Connected) {
                    handleClientInput(from, payload, payloadLen);
                }
                break;
            case SessionMessageType::TimePing:
                if (ps != nullptr && ps->connectionState == ConnectionState::Connected) {
                    handleTimePing(ps, from, payload, payloadLen);
                }
                break;
            case SessionMessageType::HelloAck:
            case SessionMessageType::GameSnapshot:
            case SessionMessageType::TimePong:
                break;
            case SessionMessageType::StateCorrection:
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

        if (config_.joinTokenU32 != 0u && hello.joinTokenU32 != config_.joinTokenU32) {
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
        std::size_t const repSlot = peerStateSlot(from);
        if (ps->connectionState == ConnectionState::Connected) {
            static_cast<void>(roster_.transition(from, ConnectionState::TimingOut));
            static_cast<void>(roster_.transition(from, ConnectionState::Disconnected));
        } else {
            static_cast<void>(roster_.transition(from, ConnectionState::Disconnected));
        }
        ps->active = false;
        ps->channel.reset();
        ps->connectionState = ConnectionState::Disconnected;
        clearPeerInterest(from);
        clearInput(from);
        clearPeerReplication(repSlot);
        if (transport_ != nullptr) {
            transport_->forgetPeer(from);
        }
    }

    void handleClientInput(PeerId from, std::uint8_t const* payload, std::size_t payloadLen) noexcept {
        if (payloadLen < kClientInputWirePayloadLegacyBytes) {
            noteMalformedEnvelope(from);
            return;
        }
        std::size_t const slot = peerStateSlot(from);
        if (slot >= MaxPlayers) {
            return;
        }
        if (peerClientInputPacketsThisTick_[slot] >= 4u) {
            return;
        }
        ClientInputWirePayload inp{};
        if (!readClientInputPayload(payload, payloadLen, inp)) {
            noteMalformedEnvelope(from);
            return;
        }
        ++peerClientInputPacketsThisTick_[slot];
        for (auto& pi : peerInputs_) {
            if (pi.peer == from) {
                pi.input = inp;
                pi.hasInput = true;
                return;
            }
        }
        for (auto& pi : peerInputs_) {
            if (pi.peer == kInvalidPeerId) {
                pi.peer = from;
                pi.input = inp;
                pi.hasInput = true;
                return;
            }
        }
    }

    void handleTimePing(PeerState* ps, PeerId to, std::uint8_t const* payload, std::size_t payloadLen) noexcept {
        SessionTimePingPayload ping{};
        if (!readSessionTimePing(payload, payloadLen, ping)) {
            return;
        }
        if (ping.clientPingId == 0u) {
            return;
        }
        sendTimePongReliable(ps, to, ping.clientPingId);
    }

    void sendTimePongReliable(PeerState* ps, PeerId to, std::uint32_t echoClientPingId) noexcept {
        SessionTimePongPayload pong{};
        pong.clientPingId = echoClientPingId;
        pong.serverSimTick = simTick_;

        std::uint16_t const seq = ps->channel.nextSequence();
        std::array<std::uint8_t, kMaxMessageBytes> buf{};
        std::size_t const len = writeSessionTimePongReliable(
            buf.data(), buf.size(), pong,
            seq, ps->channel.ackSequence(), ps->channel.ackBitmap()
        );
        if (len == 0u) {
            return;
        }
        static_cast<void>(ps->channel.recordOutgoing(seq, buf.data(), len, simTick_));
        static_cast<void>(transport_->send(to, buf.data(), len));
    }

    void sendHelloAckReliable(PeerState* ps, PeerId to, std::uint32_t clientNonce) noexcept {
        SessionHelloAckPayload ack{};
        ack.assignedPeerId = to;
        ack.serverSimTickAtAck = simTick_;
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
        for (std::size_t si = 0u; si < peerStates_.size(); ++si) {
            PeerState const& ps = peerStates_[si];
            if (!ps.active || ps.connectionState != ConnectionState::Connected) {
                continue;
            }
            emitSnapshotForPeerSlot(si, ps.peer);
        }
    }

    void emitSnapshotForPeerSlot(std::size_t peerSlot, PeerId peer) noexcept {
        InterestViewContext view{};
        InterestRegion region{};
        region.radius = defaultAoiRadius_;
        region.velocityLookaheadSeconds = aoiEntityVelocityLookaheadSeconds_;

        std::size_t viewIdx = 0u;
        for (auto const& pi : peerInterest_) {
            if (pi.peer == peer) {
                viewIdx = pi.viewEntityIndex;
                break;
            }
        }
        if (viewIdx < kMaxEntities && entities_[viewIdx].active) {
            view.position = entities_[viewIdx].position;
            view.velocity = entities_[viewIdx].velocity;
            region.viewPosition = entities_[viewIdx].position;
            if (aoiViewerPositionLookaheadSeconds_ > 0.f) {
                region.viewPosition =
                    region.viewPosition + view.velocity * aoiViewerPositionLookaheadSeconds_;
            }
        }

        struct ScoredIndex {
            std::size_t entityIndex{};
            float score{};
        };
        std::array<ScoredIndex, kMaxEntities> ranked{};
        std::size_t rankedCount = 0u;

        for (std::size_t i = 0u; i < kMaxEntities; ++i) {
            if (!entities_[i].active) {
                continue;
            }
            if (aoiEnabled_) {
                math::Vec3 const delta = entities_[i].position - region.viewPosition;
                float const dist2 = math::lengthSquared(delta);
                float effectiveR = region.radius;
                float const speed = math::length(entities_[i].velocity);
                if (speed > 0.01f) {
                    effectiveR += speed * region.velocityLookaheadSeconds;
                }
                float const effectiveR2 = effectiveR * effectiveR;
                if (dist2 > effectiveR2) {
                    continue;
                }
            }
            ranked[rankedCount++] = ScoredIndex{i, interestScoreLowerIsBetter(entities_[i], view)};
        }

        if (rankedCount == 0u) {
            return;
        }

        std::sort(ranked.begin(), ranked.begin() + rankedCount, [](ScoredIndex const& a, ScoredIndex const& b) {
            return a.score < b.score;
        });

        std::array<std::uint8_t, kTransportBufSize> inner{};
        std::size_t offset = 0u;
        std::uint32_t const budget = config_.maxSnapshotBytesPerPeer;
        std::size_t const rankedTotal = rankedCount;

        auto tryWriteEntity = [&](std::size_t ei) -> bool {
            ReplicatedEntity const& ent = entities_[ei];
            std::uint32_t const fp = quantizedKinematicsFingerprint(ent);
            // With multiple entities in-frame, skipping "unchanged" produces a partial payload; clients treat
            // each snapshot as the full replicated set for that tick (see Garden interpolator ingest).
            bool const unchanged = rankedTotal <= 1u && peerEntityEverSent_[peerSlot][ei] &&
                fp == peerLastFingerprint_[peerSlot][ei];
            if (unchanged) {
                return false;
            }
            if (budget != 0u && offset + kEntityKinematicsSnapshotWireBytes > budget) {
                return false;
            }
            EntityKinematicsSnapshot snap{};
            snap.simTick = simTick_;
            snap.entity = ent.entity;
            snap.tier = ent.tier;
            snap.positionLocal = ent.position;
            snap.linearVelocity = ent.velocity;
            snap.yawRadians = ent.yawRadians;
            std::size_t const written =
                writeEntityKinematicsSnapshot(inner.data() + offset, inner.size() - offset, snap);
            if (written == 0u) {
                return false;
            }
            offset += written;
            peerLastFingerprint_[peerSlot][ei] = fp;
            peerEntityEverSent_[peerSlot][ei] = true;
            return true;
        };

        for (std::size_t r = 0u; r < rankedCount; ++r) {
            static_cast<void>(tryWriteEntity(ranked[r].entityIndex));
        }

        if (offset == 0u) {
            std::size_t const ei = ranked[0].entityIndex;
            ReplicatedEntity const& ent = entities_[ei];
            if (budget != 0u && kEntityKinematicsSnapshotWireBytes > budget) {
                return;
            }
            EntityKinematicsSnapshot snap{};
            snap.simTick = simTick_;
            snap.entity = ent.entity;
            snap.tier = ent.tier;
            snap.positionLocal = ent.position;
            snap.linearVelocity = ent.velocity;
            snap.yawRadians = ent.yawRadians;
            std::size_t const written = writeEntityKinematicsSnapshot(inner.data(), inner.size(), snap);
            if (written == 0u) {
                return;
            }
            offset += written;
            peerLastFingerprint_[peerSlot][ei] = quantizedKinematicsFingerprint(ent);
            peerEntityEverSent_[peerSlot][ei] = true;
        }
        if (offset == 0u) {
            return;
        }

        std::array<std::uint8_t, kTransportBufSize> frame{};
        std::size_t const frameLen =
            writeSessionGameSnapshot(frame.data(), frame.size(), inner.data(), offset);
        if (frameLen > 0u) {
            static_cast<void>(transport_->send(peer, frame.data(), frameLen));
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

    std::array<PeerState, MaxPlayers> peerStates_{};
    std::array<ReplicatedEntity, kMaxEntities> entities_{};

    bool aoiEnabled_{};
    float defaultAoiRadius_{500.f};
    float aoiEntityVelocityLookaheadSeconds_{0.5f};
    float aoiViewerPositionLookaheadSeconds_{0.f};
    std::array<PeerInterestEntry, MaxPlayers> peerInterest_{};

    struct PeerInputEntry {
        PeerId peer{kInvalidPeerId};
        ClientInputWirePayload input{};
        bool hasInput{};
    };
    std::array<PeerInputEntry, MaxPlayers> peerInputs_{};

    std::array<std::array<std::uint32_t, kMaxEntities>, MaxPlayers> peerLastFingerprint_{};
    std::array<std::array<bool, kMaxEntities>, MaxPlayers> peerEntityEverSent_{};
    std::array<std::uint8_t, MaxPlayers> peerBadPayloadCount_{};
    std::array<std::uint8_t, MaxPlayers> peerClientInputPacketsThisTick_{};
    /// Resets at end of each sim tick iteration (see [`submitSyntheticClientInput`]).
    std::uint8_t syntheticListenHostInputPacketsQueued_{};
};

} // namespace marble::gameplay
