// Edge-case and regression tests for session envelopes and wire payloads
// (state correction, client input v1/v2, malformed envelopes). Complements
// multiplayer_session_envelope_test, reliable_envelope_test, multiplayer_wire_format_test.

#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

int main() {
    using namespace marble::gameplay;

    // --- SessionStateCorrection: round-trip through reliable envelope ---
    {
        std::array<std::uint8_t, 128> buf{};
        SessionStateCorrectionPayload in{};
        in.serverSimTick = 0x11223344u;
        in.entity.guid = 0x8877665544332211ull;
        in.positionLocal = {1.f, -2.f, 3.25f};
        in.linearVelocity = {0.1f, 0.f, -9.f};
        in.yawRadians = 1.5707963f;
        std::size_t const n = writeSessionStateCorrectionReliable(
            buf.data(), buf.size(), in, 3u, 4u, 0xabcdef01u);
        if (n != kSessionEnvelopeBytes + kReliableHeaderBytes + kSessionStateCorrectionPayloadBytes) {
            return 1;
        }
        SessionMessageType t{};
        std::uint8_t flags{};
        std::uint8_t const* pl{};
        std::size_t plen{};
        if (!parseSessionEnvelopeEx(buf.data(), n, t, flags, pl, plen)) {
            return 2;
        }
        if (t != SessionMessageType::StateCorrection || (flags & kSessionEnvelopeFlag_Reliable) == 0u) {
            return 3;
        }
        std::uint16_t seq{};
        std::uint16_t ackSeq{};
        std::uint32_t ackBits{};
        if (!readReliableHeader(pl, plen, seq, ackSeq, ackBits)) {
            return 4;
        }
        if (seq != 3u || ackSeq != 4u || ackBits != 0xabcdef01u) {
            return 5;
        }
        pl += kReliableHeaderBytes;
        plen -= kReliableHeaderBytes;
        SessionStateCorrectionPayload out{};
        if (!readSessionStateCorrection(pl, plen, out)) {
            return 6;
        }
        if (out.serverSimTick != in.serverSimTick || out.entity.guid != in.entity.guid ||
            std::fabs(out.positionLocal.y - in.positionLocal.y) > 1e-5f ||
            std::fabs(out.linearVelocity.z - in.linearVelocity.z) > 1e-5f ||
            std::fabs(out.yawRadians - in.yawRadians) > 1e-5f) {
            return 7;
        }
        SessionStateCorrectionPayload bad{};
        if (readSessionStateCorrection(pl, kSessionStateCorrectionPayloadBytes - 1u, bad)) {
            return 8;
        }
    }

    // --- ClientInputWirePayload: v2 round-trip, legacy 16-byte, reject <16, 17..23 uses legacy branch ---
    {
        ClientInputWirePayload v2in{};
        v2in.clientTick = 100u;
        v2in.serverTickAck = 200u;
        v2in.moveX = -0.5f;
        v2in.moveZ = 0.25f;
        v2in.steer = 0.75f;
        v2in.buttons = 0x03u;
        std::array<std::uint8_t, 32> buf{};
        if (writeClientInputPayload(buf.data(), kClientInputWirePayloadBytes - 1u, v2in) != 0u) {
            return 10;
        }
        if (writeClientInputPayload(buf.data(), buf.size(), v2in) != kClientInputWirePayloadBytes) {
            return 11;
        }
        ClientInputWirePayload v2out{};
        if (!readClientInputPayload(buf.data(), kClientInputWirePayloadBytes, v2out)) {
            return 12;
        }
        if (v2out.clientTick != v2in.clientTick || v2out.serverTickAck != v2in.serverTickAck ||
            std::fabs(v2out.moveX - v2in.moveX) > 1e-6f || std::fabs(v2out.steer - v2in.steer) > 1e-6f ||
            v2out.buttons != v2in.buttons) {
            return 13;
        }
        ClientInputWirePayload tooShort{};
        if (readClientInputPayload(buf.data(), kClientInputWirePayloadLegacyBytes - 1u, tooShort)) {
            return 14;
        }
        std::array<std::uint8_t, 32> leg{};
        writeU32Le(leg.data(), 7u);
        writeF32Le(leg.data() + 4u, 1.f);
        writeF32Le(leg.data() + 8u, 2.f);
        writeU8(leg.data() + 12u, 0x80u);
        ClientInputWirePayload legOut{};
        if (!readClientInputPayload(leg.data(), kClientInputWirePayloadLegacyBytes, legOut)) {
            return 15;
        }
        if (legOut.clientTick != 7u || legOut.serverTickAck != 0u || std::fabs(legOut.moveZ - 2.f) > 1e-6f ||
            std::fabs(legOut.steer) > 1e-6f || legOut.buttons != 0x80u) {
            return 16;
        }
        // 17 bytes: first 16 interpreted as legacy; byte 17 ignored by reader
        std::array<std::uint8_t, 32> mid{};
        std::memcpy(mid.data(), leg.data(), kClientInputWirePayloadLegacyBytes);
        mid[16] = 0xffu;
        ClientInputWirePayload midOut{};
        if (!readClientInputPayload(mid.data(), 17u, midOut)) {
            return 17;
        }
        if (midOut.clientTick != legOut.clientTick || midOut.serverTickAck != 0u) {
            return 18;
        }
    }

    // --- EntityKinematicsSnapshot: out-of-range PhysicsSimulationTier round-trips as raw byte ---
    {
        EntityKinematicsSnapshot snap{};
        snap.simTick = 1u;
        snap.entity = {2u};
        snap.tier = static_cast<PhysicsSimulationTier>(200);
        snap.positionLocal = {};
        snap.linearVelocity = {};
        snap.yawRadians = 0.f;
        std::array<std::uint8_t, 64> w{};
        std::size_t const nw = writeEntityKinematicsSnapshot(w.data(), w.size(), snap);
        if (nw != kEntityKinematicsSnapshotWireBytes) {
            return 20;
        }
        EntityKinematicsSnapshot snap2{};
        if (!readEntityKinematicsSnapshot(w.data(), nw, snap2)) {
            return 21;
        }
        if (static_cast<std::uint8_t>(snap2.tier) != 200u) {
            return 22;
        }
    }

    // --- Reliable envelope one byte short of full reliable header: parse rejects (strict framing) ---
    {
        std::array<std::uint8_t, 32> buf{};
        writeU32Le(buf.data(), kMarbleSessionMagicLe);
        writeU16Le(buf.data() + 4u, kMarbleSessionProtocolVersion);
        writeU8(buf.data() + 6u, static_cast<std::uint8_t>(SessionMessageType::Ack));
        writeU8(buf.data() + 7u, kSessionEnvelopeFlag_Reliable);
        // Only 7 bytes after envelope (need 8 for reliable header)
        std::size_t const len = kSessionEnvelopeBytes + (kReliableHeaderBytes - 1u);
        SessionMessageType t{};
        std::uint8_t flags{};
        std::uint8_t const* pl{};
        std::size_t plen{};
        if (parseSessionEnvelopeEx(buf.data(), len, t, flags, pl, plen)) {
            return 30;
        }
    }

    // --- GameSnapshot with reliable flag (never produced by writers): parse + strip header ---
    {
        EntityKinematicsSnapshot snap{};
        snap.simTick = 9u;
        snap.entity = {8u};
        snap.tier = PhysicsSimulationTier::Contact;
        std::array<std::uint8_t, 64> inner{};
        std::size_t const innerN = writeEntityKinematicsSnapshot(inner.data(), inner.size(), snap);
        if (innerN == 0u) {
            return 40;
        }
        std::array<std::uint8_t, 128> pkt{};
        std::size_t const n = writeSessionEnvelopeReliable(
            pkt.data(), pkt.size(), SessionMessageType::GameSnapshot, 1u, 2u, 3u, inner.data(), innerN);
        if (n != kSessionEnvelopeBytes + kReliableHeaderBytes + innerN) {
            return 41;
        }
        SessionMessageType t{};
        std::uint8_t flags{};
        std::uint8_t const* pl{};
        std::size_t plen{};
        if (!parseSessionEnvelopeEx(pkt.data(), n, t, flags, pl, plen)) {
            return 42;
        }
        if (t != SessionMessageType::GameSnapshot || (flags & kSessionEnvelopeFlag_Reliable) == 0u) {
            return 43;
        }
        std::uint16_t rs{};
        std::uint16_t ra{};
        std::uint32_t rb{};
        if (!readReliableHeader(pl, plen, rs, ra, rb)) {
            return 44;
        }
        pl += kReliableHeaderBytes;
        plen -= kReliableHeaderBytes;
        EntityKinematicsSnapshot out{};
        if (!readEntityKinematicsSnapshot(pl, plen, out)) {
            return 45;
        }
        if (out.simTick != snap.simTick || out.entity.guid != snap.entity.guid) {
            return 46;
        }
    }

    // --- Reserved envelope flag bits (not Reliable): still parse; Hello payload intact ---
    {
        SessionHelloPayload helloIn{};
        helloIn.clientUdpPortHost = 40000u;
        helloIn.clientNonce = 0xcafebabeu;
        helloIn.joinTokenU32 = 0u;
        std::array<std::uint8_t, kSessionHelloPayloadBytes> pl{};
        writeU16Le(pl.data(), helloIn.clientUdpPortHost);
        writeU32Le(pl.data() + 2u, helloIn.clientNonce);
        writeU32Le(pl.data() + 6u, helloIn.joinTokenU32);
        std::array<std::uint8_t, 64> buf{};
        std::uint8_t constexpr kReservedFlags = 0x06u; // bits 1-2 set, not reliable
        std::size_t const total = kSessionEnvelopeBytes + pl.size();
        if (buf.size() < total) {
            return 50;
        }
        writeU32Le(buf.data(), kMarbleSessionMagicLe);
        writeU16Le(buf.data() + 4u, kMarbleSessionProtocolVersion);
        writeU8(buf.data() + 6u, static_cast<std::uint8_t>(SessionMessageType::Hello));
        writeU8(buf.data() + 7u, kReservedFlags);
        std::memcpy(buf.data() + kSessionEnvelopeBytes, pl.data(), pl.size());
        SessionMessageType t{};
        std::uint8_t flags{};
        std::uint8_t const* pay{};
        std::size_t payLen{};
        if (!parseSessionEnvelopeEx(buf.data(), total, t, flags, pay, payLen)) {
            return 51;
        }
        if (t != SessionMessageType::Hello || flags != kReservedFlags) {
            return 52;
        }
        if ((flags & kSessionEnvelopeFlag_Reliable) != 0u) {
            return 53;
        }
        SessionHelloPayload helloOut{};
        if (!readSessionHello(pay, payLen, helloOut)) {
            return 54;
        }
        if (helloOut.clientUdpPortHost != helloIn.clientUdpPortHost || helloOut.clientNonce != helloIn.clientNonce) {
            return 55;
        }
    }

    return 0;
}
