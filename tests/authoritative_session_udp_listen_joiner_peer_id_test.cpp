// First UDP joiner is assigned peer id >= minimumJoinerPeerId (listen host uses 3 to reserve 2).

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/UdpGameTransport.hpp"

#include <array>
#include <cstdint>

int main() {
    using namespace marble::gameplay;

    UdpGameTransport server{};
    UdpGameTransport client{};

    if (!server.bind(0u) || !client.bind(0u)) {
        return 1;
    }
    server.setMinimumJoinerPeerId(3u);

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

    SessionHelloPayload hello{};
    hello.clientUdpPortHost = 0u;
    hello.clientNonce = 0xC001u;
    std::array<std::uint8_t, 64> wire{};
    std::size_t const flen = writeSessionHello(wire.data(), wire.size(), hello);
    if (flen == 0u) {
        return 5;
    }
    if (!client.sendRaw(localhostNet, serverPort, wire.data(), flen)) {
        return 6;
    }

    for (int i = 0; i < 600; ++i) {
        session.tick(1.f / 60.f);
    }

    if (session.peerCount() < 1u) {
        return 7;
    }
    if (!session.isConnected(3u)) {
        return 8;
    }
    if (session.isConnected(2u)) {
        return 9;
    }

    return 0;
}
