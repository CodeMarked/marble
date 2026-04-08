#include "gameplay/MultiplayerSessionEnvelope.hpp"

#include <array>
#include <cstdint>
#include <cstring>

int main() {
    using namespace marble::gameplay;

    // --- protocol version is 2 ---
    static_assert(kMarbleSessionProtocolVersion == 2u, "expected protocol version 2");

    // --- unreliable Hello still works (flags = 0) ---
    SessionHelloPayload helloIn{};
    helloIn.clientUdpPortHost = 54321u;
    helloIn.clientNonce = 0x11223344u;

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
        helloOut.clientNonce != helloIn.clientNonce) {
        return 6;
    }

    // --- reliable HelloAck round-trip ---
    SessionHelloAckPayload ackIn{};
    ackIn.assignedPeerId = 7u;
    ackIn.echoClientNonce = helloIn.clientNonce;

    std::array<std::uint8_t, 64> ackBuf{};
    std::size_t const nAck = writeSessionHelloAckReliable(
        ackBuf.data(), ackBuf.size(), ackIn, 1u, 0u, 0u
    );
    // envelope(8) + reliable header(8) + HelloAck payload(8) = 24
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
    if (ackOut.assignedPeerId != 7u || ackOut.echoClientNonce != helloIn.clientNonce) {
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

    return 0;
}
