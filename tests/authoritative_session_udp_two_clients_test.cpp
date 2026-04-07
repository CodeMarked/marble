// Two UDP clients send session Hello to an AuthoritativeSession on localhost; both should reach Connected.

#include "gameplay/AuthoritativeSession.hpp"
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

    std::uint32_t localhostNet{};
    if (!parseIpv4Host("127.0.0.1", localhostNet)) {
        return 2;
    }

    std::uint16_t const serverPort = server.localPort();
    if (serverPort == 0u) {
        return 3;
    }

    AuthoritativeSession<> session{};
    SessionConfig config{};
    config.mode = MultiplayerMode::DedicatedServer;
    config.maxPlayers = 4u;
    config.simulationHz = 60u;
    config.snapshotHz = 20u;
    if (!session.initialize(config, &server)) {
        return 4;
    }

    auto sendHello = [&](UdpGameTransport& from, std::uint32_t nonce) -> bool {
        SessionHelloPayload hello{};
        hello.clientUdpPortHost = 0u;
        hello.clientNonce = nonce;
        std::array<std::uint8_t, 64> wire{};
        std::size_t const flen = writeSessionHello(wire.data(), wire.size(), hello);
        if (flen == 0u) {
            return false;
        }
        return from.sendRaw(localhostNet, serverPort, wire.data(), flen);
    };

    if (!sendHello(clientA, 0xA001u) || !sendHello(clientB, 0xB002u)) {
        return 5;
    }

    for (int i = 0; i < 600; ++i) {
        session.tick(1.f / 60.f);
    }

    if (session.peerCount() < 2u) {
        return 6;
    }
    if (!session.isConnected(2u) || !session.isConnected(3u)) {
        return 7;
    }

    return 0;
}
