#include "gameplay/MultiplayerSessionEnvelope.hpp"

#include <array>
#include <cstdint>
#include <cstring>

int main() {
    using namespace marble::gameplay;

    static_assert(kMarbleSessionProtocolVersion == 5u, "expected protocol version 5");

    // --- unreliable Hello still works (flags = 0) ---
    SessionHelloPayload helloIn{};
    helloIn.clientUdpPortHost = 54321u;
    helloIn.clientNonce = 0x11223344u;
    helloIn.joinTokenU32 = 0x99aabbccu;

    std::array<std::uint8_t, 64> buf{};
    std::size_t const nHello = writeSessionHello(buf.data(), buf.size(), helloIn);
    if (nHello != kSessionEnvelopeBytes + kSessionHelloPayloadBytes) {
        return 2;
    }

    SessionMessageType t{};
    std::uint8_t flags{};
    std::uint8_t const* pl{};
    std::size_t plen{};
    if (!parseSessionEnvelopeEx(buf.data(), nHello, t, flags, pl, plen)) {
        return 3;
    }
    if (t != SessionMessageType::Hello || flags != 0u) {
        return 4;
    }
    SessionHelloPayload helloOut{};
    if (!readSessionHello(pl, plen, helloOut)) {
        return 5;
    }
    if (helloOut.clientUdpPortHost != helloIn.clientUdpPortHost ||
        helloOut.clientNonce != helloIn.clientNonce || helloOut.joinTokenU32 != helloIn.joinTokenU32) {
        return 6;
    }

    // --- reliable HelloAck round-trip ---
    SessionHelloAckPayload ackIn{};
    ackIn.assignedPeerId = 7u;
    ackIn.serverSimTickAtAck = 99u;
    ackIn.echoClientNonce = helloIn.clientNonce;

    std::array<std::uint8_t, 64> ackBuf{};
    std::size_t const nAck = writeSessionHelloAckReliable(
        ackBuf.data(), ackBuf.size(), ackIn, 1u, 0u, 0u
    );
    // envelope(8) + reliable header(8) + HelloAck payload(12) = 28
    if (nAck != kSessionEnvelopeBytes + kReliableHeaderBytes + kSessionHelloAckPayloadBytes) {
        return 7;
    }

    if (!parseSessionEnvelopeEx(ackBuf.data(), nAck, t, flags, pl, plen)) {
        return 8;
    }
    if (t != SessionMessageType::HelloAck) {
        return 9;
    }
    if ((flags & kSessionEnvelopeFlag_Reliable) == 0u) {
        return 10;
    }

    // read reliable header
    std::uint16_t seq{};
    std::uint16_t ackSeq{};
    std::uint32_t ackBits{};
    if (!readReliableHeader(pl, plen, seq, ackSeq, ackBits)) {
        return 11;
    }
    if (seq != 1u || ackSeq != 0u || ackBits != 0u) {
        return 12;
    }

    // payload is after reliable header
    std::uint8_t const* innerPl = pl + kReliableHeaderBytes;
    std::size_t const innerLen = plen - kReliableHeaderBytes;
    SessionHelloAckPayload ackOut{};
    if (!readSessionHelloAck(innerPl, innerLen, ackOut)) {
        return 13;
    }
    if (ackOut.assignedPeerId != 7u || ackOut.serverSimTickAtAck != 99u ||
        ackOut.echoClientNonce != helloIn.clientNonce) {
        return 14;
    }

    // --- pure Ack message ---
    std::array<std::uint8_t, 32> ackOnlyBuf{};
    std::size_t const nAckOnly = writeSessionAck(ackOnlyBuf.data(), ackOnlyBuf.size(), 5u, 3u, 0x07u);
    // envelope(8) + reliable header(8) + no payload = 16
    if (nAckOnly != kSessionEnvelopeBytes + kReliableHeaderBytes) {
        return 15;
    }
    if (!parseSessionEnvelopeEx(ackOnlyBuf.data(), nAckOnly, t, flags, pl, plen)) {
        return 16;
    }
    if (t != SessionMessageType::Ack) {
        return 17;
    }
    if ((flags & kSessionEnvelopeFlag_Reliable) == 0u) {
        return 18;
    }
    if (!readReliableHeader(pl, plen, seq, ackSeq, ackBits)) {
        return 19;
    }
    if (seq != 5u || ackSeq != 3u || ackBits != 0x07u) {
        return 20;
    }

    // --- Disconnect message ---
    std::array<std::uint8_t, 32> discBuf{};
    std::size_t const nDisc = writeSessionDisconnect(discBuf.data(), discBuf.size(), 10u, 8u, 0xFFu);
    if (nDisc != kSessionEnvelopeBytes + kReliableHeaderBytes) {
        return 21;
    }
    if (!parseSessionEnvelopeEx(discBuf.data(), nDisc, t, flags, pl, plen)) {
        return 22;
    }
    if (t != SessionMessageType::Disconnect) {
        return 23;
    }
    if (!readReliableHeader(pl, plen, seq, ackSeq, ackBits)) {
        return 24;
    }
    if (seq != 10u || ackSeq != 8u || ackBits != 0xFFu) {
        return 25;
    }

    // --- GameSnapshot still works (unreliable, flags = 0) ---
    std::array<std::uint8_t, 8> inner{1, 2, 3, 4, 5, 6, 7, 8};
    std::array<std::uint8_t, 32> snapBuf{};
    std::size_t const nSnap = writeSessionGameSnapshot(snapBuf.data(), snapBuf.size(), inner.data(), inner.size());
    if (nSnap != kSessionEnvelopeBytes + 8u) {
        return 26;
    }
    if (!parseSessionEnvelopeEx(snapBuf.data(), nSnap, t, flags, pl, plen)) {
        return 27;
    }
    if (t != SessionMessageType::GameSnapshot || flags != 0u) {
        return 28;
    }
    if (std::memcmp(pl, inner.data(), inner.size()) != 0) {
        return 29;
    }

    // --- reliable TimePing / TimePong (protocol v4) ---
    SessionTimePingPayload pingIn{};
    pingIn.clientPingId = 0x55aa6601u;
    std::array<std::uint8_t, 64> pingBuf{};
    std::size_t const nPing = writeSessionTimePingReliable(
        pingBuf.data(), pingBuf.size(), pingIn, 3u, 2u, 0xCu);
    if (nPing != kSessionEnvelopeBytes + kReliableHeaderBytes + kSessionTimePingPayloadBytes) {
        return 40;
    }
    if (!parseSessionEnvelopeEx(pingBuf.data(), nPing, t, flags, pl, plen)) {
        return 41;
    }
    if (t != SessionMessageType::TimePing || (flags & kSessionEnvelopeFlag_Reliable) == 0u) {
        return 42;
    }
    if (!readReliableHeader(pl, plen, seq, ackSeq, ackBits)) {
        return 43;
    }
    if (seq != 3u || ackSeq != 2u || ackBits != 0xCu) {
        return 44;
    }
    {
        std::uint8_t const* innerPing = pl + kReliableHeaderBytes;
        std::size_t const innerPingLen = plen - kReliableHeaderBytes;
        SessionTimePingPayload pingOut{};
        if (!readSessionTimePing(innerPing, innerPingLen, pingOut) ||
            pingOut.clientPingId != pingIn.clientPingId) {
            return 45;
        }
    }

    SessionTimePongPayload pongIn{};
    pongIn.clientPingId = pingIn.clientPingId;
    pongIn.serverSimTick = 1001u;
    std::array<std::uint8_t, 64> pongBuf{};
    std::size_t const nPong = writeSessionTimePongReliable(
        pongBuf.data(), pongBuf.size(), pongIn, 4u, 3u, 0u);
    if (nPong != kSessionEnvelopeBytes + kReliableHeaderBytes + kSessionTimePongPayloadBytes) {
        return 46;
    }
    if (!parseSessionEnvelopeEx(pongBuf.data(), nPong, t, flags, pl, plen)) {
        return 47;
    }
    if (t != SessionMessageType::TimePong) {
        return 48;
    }
    if (!readReliableHeader(pl, plen, seq, ackSeq, ackBits)) {
        return 49;
    }
    if (seq != 4u) {
        return 50;
    }
    {
        std::uint8_t const* innerPong = pl + kReliableHeaderBytes;
        std::size_t const innerPongLen = plen - kReliableHeaderBytes;
        SessionTimePongPayload pongOut{};
        if (!readSessionTimePong(innerPong, innerPongLen, pongOut)) {
            return 51;
        }
        if (pongOut.clientPingId != pongIn.clientPingId || pongOut.serverSimTick != 1001u) {
            return 52;
        }
    }

    // --- reject undersized envelope ---
    if (parseSessionEnvelopeEx(buf.data(), kSessionEnvelopeBytes - 1u, t, flags, pl, plen)) {
        return 30;
    }

    // --- reject corrupted magic ---
    buf[0] = 0;
    if (parseSessionEnvelopeEx(buf.data(), nHello, t, flags, pl, plen)) {
        return 31;
    }

    // --- reject old protocol version (write version 1 manually) ---
    std::array<std::uint8_t, 16> v1Buf{};
    writeU32Le(v1Buf.data(), kMarbleSessionMagicLe);
    writeU16Le(v1Buf.data() + 4u, 1u); // version 1
    writeU8(v1Buf.data() + 6u, 1u);
    writeU8(v1Buf.data() + 7u, 0u);
    if (parseSessionEnvelopeEx(v1Buf.data(), 8u, t, flags, pl, plen)) {
        return 32; // must reject version 1
    }

    // --- readSessionTimePing / readSessionTimePong reject truncated payloads ---
    {
        std::array<std::uint8_t, 4> shortPing{};
        writeU32Le(shortPing.data(), 0x11223344u);
        SessionTimePingPayload pingTmp{};
        if (readSessionTimePing(shortPing.data(), kSessionTimePingPayloadBytes - 1u, pingTmp)) {
            return 53;
        }
        std::array<std::uint8_t, 8> shortPong{};
        writeU32Le(shortPong.data(), 1u);
        writeU32Le(shortPong.data() + 4u, 2u);
        SessionTimePongPayload pongTmp{};
        if (readSessionTimePong(shortPong.data(), kSessionTimePongPayloadBytes - 1u, pongTmp)) {
            return 54;
        }
    }

    return 0;
}
