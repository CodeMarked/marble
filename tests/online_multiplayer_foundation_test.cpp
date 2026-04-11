#include "gameplay/OnlineMultiplayerFoundation.hpp"

#include <array>

int main() {
    using namespace marble::gameplay;

    if (!isValid(SessionConfig{MultiplayerMode::Offline, 1u, 60u, 20u, 2u})) {
        return 1;
    }
    if (isValid(SessionConfig{MultiplayerMode::Offline, 2u, 60u, 20u, 2u})) {
        return 2;
    }
    if (!isValid(SessionConfig{MultiplayerMode::ListenServer, 4u, 60u, 20u, 2u})) {
        return 3;
    }
    if (isValid(SessionConfig{MultiplayerMode::DedicatedServer, 8u, 20u, 30u, 2u})) {
        return 4;
    }

    if (isValid(SessionConfig{MultiplayerMode::DedicatedServer, 8u, 60u, 25u, 2u})) {
        return 35;
    }
    if (isValid(SessionConfig{MultiplayerMode::DedicatedServer, 8u, 60u, 16u, 2u})) {
        return 36;
    }
    if (!isValid(SessionConfig{MultiplayerMode::DedicatedServer, 8u, 60u, 30u, 2u})) {
        return 37;
    }

    if (!connectionTransitionAllowed(ConnectionState::Disconnected, ConnectionState::Connecting)) {
        return 5;
    }
    if (connectionTransitionAllowed(ConnectionState::Disconnected, ConnectionState::Connected)) {
        return 6;
    }
    if (!connectionTransitionAllowed(ConnectionState::Connecting, ConnectionState::Handshaking)) {
        return 7;
    }
    if (!connectionTransitionAllowed(ConnectionState::Connected, ConnectionState::TimingOut)) {
        return 8;
    }

    AuthorityRoster<4> roster{};
    if (!roster.bootstrap(MultiplayerMode::ListenServer)) {
        return 9;
    }
    if (!roster.isAuthoritativePeer(1u)) {
        return 10;
    }
    if (roster.connectedCount() != 1u) {
        return 11;
    }
    if (!roster.connectPeer(10u)) {
        return 12;
    }
    if (!roster.transition(10u, ConnectionState::Handshaking)) {
        return 13;
    }
    if (!roster.transition(10u, ConnectionState::Connected)) {
        return 14;
    }
    if (roster.connectedCount() != 2u) {
        return 15;
    }
    if (!roster.setLastInputTick(10u, 12u)) {
        return 16;
    }
    if (roster.setLastInputTick(10u, 11u)) {
        return 17;
    }
    if (!roster.transition(10u, ConnectionState::TimingOut)) {
        return 18;
    }
    if (!roster.transition(10u, ConnectionState::Disconnected)) {
        return 19;
    }
    if (roster.connectedCount() != 1u) {
        return 20;
    }

    AuthorityRoster<3> dedicated{};
    if (!dedicated.bootstrap(MultiplayerMode::DedicatedServer)) {
        return 21;
    }
    if (dedicated.connectedCount() != 0u) {
        return 22;
    }
    if (dedicated.addAuthorityPeer(2u)) {
        return 23;
    }

    InputCommandQueue<4> q{};
    if (!q.push({1u, 5u, 1, 0, 1u})) {
        return 24;
    }
    if (!q.push({2u, 6u, 0, 1, 2u})) {
        return 25;
    }
    if (!q.push({3u, 5u, -1, 0, 0u})) {
        return 26;
    }
    std::array<InputCommand, 4> out{};
    const std::size_t hits = q.drainTick(5u, out.data(), out.size());
    if (hits != 2u) {
        return 27;
    }
    if (q.size() != 1u) {
        return 28;
    }
    if (out[0].peer != 1u || out[1].peer != 3u) {
        return 29;
    }

    InputCommandQueue<1> cap{};
    if (!cap.push({1u, 1u, 0, 0, 0u})) {
        return 30;
    }
    if (cap.push({1u, 2u, 0, 0, 0u})) {
        return 31;
    }
    if (cap.push({kInvalidPeerId, 3u, 0, 0, 0u})) {
        return 32;
    }

    const ReplicationMessageMeta m1{ReplicationChannel::StateDelta, ReplicationReliability::Unreliable, 100u, 1u};
    const ReplicationMessageMeta m2{ReplicationChannel::StateDelta, ReplicationReliability::Reliable, 100u, 2u};
    const ReplicationMessageMeta m3{ReplicationChannel::Control, ReplicationReliability::Unreliable, 100u, 3u};
    if (requiresAck(m1)) {
        return 33;
    }
    if (!requiresAck(m2) || !requiresAck(m3)) {
        return 34;
    }

    return 0;
}
