#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"

#include <array>
#include <cstring>

int main() {
    using namespace marble::gameplay;

    SessionHelloPayload helloIn{};
    helloIn.clientUdpPortHost = 54321u;
    helloIn.clientNonce = 0x11223344u;

    std::array<std::uint8_t, 64> buf{};
    std::size_t const nHello = writeSessionHello(buf.data(), buf.size(), helloIn);
    if (nHello != kSessionEnvelopeBytes + kSessionHelloPayloadBytes) {
        return 1;
    }

    SessionMessageType t{};
    std::uint8_t const* pl{};
    std::size_t plen{};
    if (!parseSessionEnvelope(buf.data(), nHello, t, pl, plen)) {
        return 2;
    }
    if (t != SessionMessageType::Hello) {
        return 3;
    }
    SessionHelloPayload helloOut{};
    if (!readSessionHello(pl, plen, helloOut)) {
        return 4;
    }
    if (helloOut.clientUdpPortHost != helloIn.clientUdpPortHost || helloOut.clientNonce != helloIn.clientNonce) {
        return 5;
    }

    SessionHelloAckPayload ackIn{};
    ackIn.assignedPeerId = 7u;
    ackIn.echoClientNonce = helloIn.clientNonce;
    std::size_t const nAck = writeSessionHelloAck(buf.data(), buf.size(), ackIn);
    if (nAck != kSessionEnvelopeBytes + kSessionHelloAckPayloadBytes) {
        return 6;
    }
    if (!parseSessionEnvelope(buf.data(), nAck, t, pl, plen) || t != SessionMessageType::HelloAck) {
        return 7;
    }
    SessionHelloAckPayload ackOut{};
    if (!readSessionHelloAck(pl, plen, ackOut)) {
        return 8;
    }
    if (ackOut.assignedPeerId != 7u || ackOut.echoClientNonce != helloIn.clientNonce) {
        return 9;
    }

    EntityKinematicsSnapshot snap{};
    snap.simTick = 3u;
    snap.entity = {99u};
    snap.tier = PhysicsSimulationTier::Contact;
    snap.positionLocal = {1.f, 2.f, 3.f};
    snap.linearVelocity = {4.f, 5.f, 6.f};

    std::array<std::uint8_t, 128> inner{};
    std::size_t const innerLen = writeEntityKinematicsSnapshot(inner.data(), inner.size(), snap);
    if (innerLen == 0u) {
        return 10;
    }
    std::size_t const nSnap = writeSessionGameSnapshot(buf.data(), buf.size(), inner.data(), innerLen);
    if (nSnap != kSessionEnvelopeBytes + innerLen) {
        return 11;
    }
    std::uint8_t const* gamePl{};
    std::size_t gameLen{};
    if (!parseSessionGameSnapshot(buf.data(), nSnap, gamePl, gameLen) || gameLen != innerLen) {
        return 12;
    }
    if (std::memcmp(gamePl, inner.data(), innerLen) != 0) {
        return 13;
    }

    if (parseSessionEnvelope(buf.data(), kSessionEnvelopeBytes - 1u, t, pl, plen)) {
        return 14;
    }

    buf[0] = 0;
    if (parseSessionEnvelope(buf.data(), nHello, t, pl, plen)) {
        return 15;
    }

    return 0;
}
