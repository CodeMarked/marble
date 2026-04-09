// Verifies UdpGameTransport assigns PeerIds for unknown sources that send Session Hello and delivers via receive().

#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/UdpGameTransport.hpp"

#include <array>
#include <cstdint>

int main() {
    using namespace marble::gameplay;

    UdpGameTransport server{};
    UdpGameTransport clientA{};
    UdpGameTransport clientB{};

    if (!server.bind(0u) || !clientA.bind(0u) || !clientB.bind(0u)) {
        return 1;
    }

    std::uint16_t const serverPort = server.localPort();
    if (serverPort == 0u) {
        return 2;
    }

    std::uint32_t localhostNet{};
    if (!parseIpv4Host("127.0.0.1", localhostNet)) {
        return 3;
    }

    {
        SessionHelloPayload hello{};
        hello.clientUdpPortHost = 0u;
        hello.clientNonce = 0x111u;
        std::array<std::uint8_t, 64> wire{};
        std::size_t const flen = writeSessionHello(wire.data(), wire.size(), hello);
        if (flen == 0u) {
            return 4;
        }
        if (!clientA.sendRaw(localhostNet, serverPort, wire.data(), flen)) {
            return 5;
        }
    }

    PeerId from{kInvalidPeerId};
    std::array<std::uint8_t, 2048> rx{};
    constexpr int kMaxSpins = 100000;
    std::size_t n1 = 0u;
    for (int spin = 0; spin < kMaxSpins; ++spin) {
        n1 = server.receive(from, rx.data(), rx.size());
        if (n1 != 0u) {
            break;
        }
    }
    if (n1 == 0u || from != 2u) {
        return 6;
    }
    SessionMessageType msg1{};
    std::uint8_t flags1{};
    std::uint8_t const* pay1{};
    std::size_t len1{};
    if (!parseSessionEnvelopeEx(rx.data(), n1, msg1, flags1, pay1, len1) ||
        msg1 != SessionMessageType::Hello) {
        return 7;
    }
    SessionHelloPayload got1{};
    if (!readSessionHello(pay1, len1, got1) || got1.clientNonce != 0x111u) {
        return 8;
    }

    {
        SessionHelloPayload hello{};
        hello.clientUdpPortHost = 0u;
        hello.clientNonce = 0x222u;
        std::array<std::uint8_t, 64> wire{};
        std::size_t const flen = writeSessionHello(wire.data(), wire.size(), hello);
        if (flen == 0u || !clientB.sendRaw(localhostNet, serverPort, wire.data(), flen)) {
            return 9;
        }
    }

    std::size_t n2 = 0u;
    for (int spin = 0; spin < kMaxSpins; ++spin) {
        n2 = server.receive(from, rx.data(), rx.size());
        if (n2 != 0u) {
            break;
        }
    }
    if (n2 == 0u || from != 3u) {
        return 10;
    }
    SessionMessageType msg2{};
    std::uint8_t flags2{};
    std::uint8_t const* pay2{};
    std::size_t len2{};
    if (!parseSessionEnvelopeEx(rx.data(), n2, msg2, flags2, pay2, len2) ||
        msg2 != SessionMessageType::Hello) {
        return 11;
    }
    SessionHelloPayload got2{};
    if (!readSessionHello(pay2, len2, got2) || got2.clientNonce != 0x222u) {
        return 12;
    }

    return 0;
}
