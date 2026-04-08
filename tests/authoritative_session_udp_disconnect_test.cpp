// ClientSession disconnect should drop the peer on the authoritative server (forgetPeer + roster).

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/UdpGameTransport.hpp"

#include <cstdint>

int main() {
    using namespace marble::gameplay;

    UdpGameTransport server{};
    UdpGameTransport client{};

    if (!server.bind(0u) || !client.bind(0u)) {
        return 1;
    }

    std::uint16_t const serverPort = server.localPort();
    if (serverPort == 0u) {
        return 2;
    }

    if (!client.addPeer(1u, "127.0.0.1", serverPort)) {
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

    ClientSession<> user{};
    if (!user.initialize(&client, 1u, 60u, 0xD15Cu)) {
        return 5;
    }

    for (int i = 0; i < 900; ++i) {
        session.tick(1.f / 60.f);
        user.tick(1.f / 60.f);
        if (user.state() == ConnectionState::Connected) {
            break;
        }
    }

    if (user.state() != ConnectionState::Connected || session.peerCount() == 0u) {
        return 6;
    }

    user.disconnect();

    for (int i = 0; i < 600; ++i) {
        session.tick(1.f / 60.f);
    }

    if (session.peerCount() != 0u) {
        return 7;
    }

    return 0;
}
