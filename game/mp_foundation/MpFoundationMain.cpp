// Phase 3 narrow sample: UDP + session envelope (magic/version 2) + handshake + kinematics snapshots.
// One socket for all datagrams (security: reject wrong magic/version before parsing body).
// Protocol version 2: reliable framing available; this sample still uses unreliable handshake/snapshots.

#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/UdpGameTransport.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

using namespace marble::gameplay;

inline constexpr PeerId kServerPeerId = 1u;
inline constexpr PeerId kClientPeerId = 2u;
inline constexpr std::uint16_t kDefaultPort = 27777u;

[[nodiscard]] int usage() {
    std::fprintf(
        stderr,
        "mp_foundation — Marble Phase 3 transport sample (single UDP socket, framed messages)\n"
        "  server: mp_foundation --server [--port N] [--snapshot-ticks M]   (default port %u, ticks 120)\n"
        "  client: mp_foundation --client --host ADDR [--port N] [--expect-ticks M]\n",
        static_cast<unsigned>(kDefaultPort)
    );
    return 2;
}

[[nodiscard]] bool parseU16(char const* s, std::uint16_t& out) {
    if (s == nullptr || s[0] == '\0') {
        return false;
    }
    char* end{};
    unsigned long const v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0' || v > 65535ul) {
        return false;
    }
    out = static_cast<std::uint16_t>(v);
    return true;
}

[[nodiscard]] int runServer(std::uint16_t bindPort, std::uint32_t snapshotTicks) {
    UdpGameTransport transport{};
    if (!transport.bind(bindPort)) {
        std::fprintf(stderr, "server: bind failed on port %u\n", static_cast<unsigned>(bindPort));
        return 1;
    }
    std::fprintf(
        stderr,
        "server: listening UDP %u (peer ids: server=%u client=%u)\n",
        static_cast<unsigned>(transport.localPort()),
        static_cast<unsigned>(kServerPeerId),
        static_cast<unsigned>(kClientPeerId)
    );

    bool hasClient = false;
    std::uint32_t simTick = 0u;
    std::uint32_t snapshotsSent = 0u;

    auto lastSnapshot = std::chrono::steady_clock::now();
    constexpr auto kSnapshotPeriod = std::chrono::milliseconds(50);
    auto const bootTime = std::chrono::steady_clock::now();

    std::array<std::uint8_t, 2048> rxBuf{};
    std::array<std::uint8_t, 2048> txBuf{};

    while (true) {
        if (!hasClient && std::chrono::steady_clock::now() - bootTime > std::chrono::seconds(30)) {
            std::fprintf(stderr, "server: handshake timeout (no client in 30s)\n");
            return 1;
        }
        if (hasClient && snapshotsSent >= snapshotTicks) {
            break;
        }
        // Handshake + future input: accept any datagram first.
        std::uint32_t srcIp{};
        std::uint16_t srcPort{};
        std::size_t const n = transport.receiveRaw(srcIp, srcPort, rxBuf.data(), rxBuf.size());
        if (n > 0u) {
            SessionMessageType msgType{};
            std::uint8_t const* payload{};
            std::size_t payloadLen{};
            if (!parseSessionEnvelope(rxBuf.data(), n, msgType, payload, payloadLen)) {
                continue;
            }
            if (!hasClient && msgType == SessionMessageType::Hello) {
                SessionHelloPayload hello{};
                if (!readSessionHello(payload, payloadLen, hello)) {
                    continue;
                }
                if (hello.clientUdpPortHost != srcPort) {
                    // Basic source/port consistency (spoof hardening for LAN sample).
                    continue;
                }
                if (!transport.addPeerEndpoint(kClientPeerId, srcIp, srcPort)) {
                    std::fprintf(stderr, "server: peer table full\n");
                    return 1;
                }
                SessionHelloAckPayload ack{};
                ack.assignedPeerId = kClientPeerId;
                ack.echoClientNonce = hello.clientNonce;
                std::size_t const ackLen = writeSessionHelloAck(txBuf.data(), txBuf.size(), ack);
                if (ackLen == 0u || !transport.sendRaw(srcIp, srcPort, txBuf.data(), ackLen)) {
                    std::fprintf(stderr, "server: HelloAck send failed\n");
                    return 1;
                }
                hasClient = true;
                std::fprintf(stderr, "server: client handshake ok (nonce=%08x)\n", hello.clientNonce);
                lastSnapshot = std::chrono::steady_clock::now();
            }
        }

        auto const now = std::chrono::steady_clock::now();
        if (hasClient && snapshotsSent < snapshotTicks && (now - lastSnapshot) >= kSnapshotPeriod) {
            lastSnapshot = now;
            ++simTick;

            EntityKinematicsSnapshot snap{};
            snap.simTick = simTick;
            snap.entity = {0x1000u};
            snap.tier = PhysicsSimulationTier::Contact;
            snap.positionLocal = {static_cast<float>(simTick) * 0.01f, 0.f, 0.f};
            snap.linearVelocity = {1.f, 0.f, 0.f};

            std::array<std::uint8_t, 128> inner{};
            std::size_t const innerLen = writeEntityKinematicsSnapshot(inner.data(), inner.size(), snap);
            if (innerLen == 0u) {
                return 1;
            }
            std::size_t const frameLen = writeSessionGameSnapshot(txBuf.data(), txBuf.size(), inner.data(), innerLen);
            if (frameLen == 0u || !transport.send(kClientPeerId, txBuf.data(), frameLen)) {
                std::fprintf(stderr, "server: snapshot send failed at tick %u\n", simTick);
                return 1;
            }
            ++snapshotsSent;
            if ((snapshotsSent % 20u) == 0u) {
                std::fprintf(stderr, "server: sent %u snapshots (simTick=%u)\n", snapshotsSent, simTick);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::fprintf(stderr, "server: done (sent %u snapshots)\n", snapshotsSent);
    return 0;
}

[[nodiscard]] int runClient(char const* host, std::uint16_t serverPort, std::uint32_t expectTicks) {
    UdpGameTransport transport{};
    if (!transport.bind(0u)) {
        std::fprintf(stderr, "client: bind failed\n");
        return 1;
    }
    if (!transport.addPeer(kServerPeerId, host, serverPort)) {
        std::fprintf(stderr, "client: addPeer server failed\n");
        return 1;
    }

    std::uint32_t const nonce = 0xdeadbeefu;
    SessionHelloPayload hello{};
    hello.clientUdpPortHost = transport.localPort();
    hello.clientNonce = nonce;

    std::array<std::uint8_t, 256> txBuf{};
    std::size_t const helloLen = writeSessionHello(txBuf.data(), txBuf.size(), hello);
    if (helloLen == 0u || !transport.send(kServerPeerId, txBuf.data(), helloLen)) {
        std::fprintf(stderr, "client: Hello send failed\n");
        return 1;
    }
    std::fprintf(
        stderr,
        "client: Hello sent to %s:%u (local udp %u)\n",
        host,
        static_cast<unsigned>(serverPort),
        static_cast<unsigned>(transport.localPort())
    );

    bool acked = false;
    std::array<std::uint8_t, 2048> rxBuf{};
    auto const handshakeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

    while (!acked && std::chrono::steady_clock::now() < handshakeDeadline) {
        PeerId from{};
        std::size_t const n = transport.receive(from, rxBuf.data(), rxBuf.size());
        if (n == 0u) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (from != kServerPeerId) {
            continue;
        }
        SessionMessageType t{};
        std::uint8_t const* pl{};
        std::size_t plen{};
        if (!parseSessionEnvelope(rxBuf.data(), n, t, pl, plen) || t != SessionMessageType::HelloAck) {
            continue;
        }
        SessionHelloAckPayload ack{};
        if (!readSessionHelloAck(pl, plen, ack) || ack.echoClientNonce != nonce) {
            std::fprintf(stderr, "client: bad HelloAck\n");
            return 1;
        }
        if (ack.assignedPeerId != kClientPeerId) {
            std::fprintf(stderr, "client: unexpected assigned peer id %u\n", static_cast<unsigned>(ack.assignedPeerId));
            return 1;
        }
        acked = true;
        std::fprintf(stderr, "client: HelloAck ok (assigned peer %u)\n", static_cast<unsigned>(ack.assignedPeerId));
    }

    if (!acked) {
        std::fprintf(stderr, "client: handshake timeout\n");
        return 1;
    }

    std::uint32_t received = 0u;
    auto const snapshotDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

    while (received < expectTicks) {
        PeerId from{};
        std::size_t const n = transport.receive(from, rxBuf.data(), rxBuf.size());
        if (n == 0u) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (std::chrono::steady_clock::now() > snapshotDeadline) {
                std::fprintf(stderr, "client: snapshot receive timeout\n");
                return 1;
            }
            continue;
        }
        if (from != kServerPeerId) {
            continue;
        }
        std::uint8_t const* gamePl{};
        std::size_t gameLen{};
        if (!parseSessionGameSnapshot(rxBuf.data(), n, gamePl, gameLen)) {
            continue;
        }
        EntityKinematicsSnapshot snap{};
        if (!readEntityKinematicsSnapshot(gamePl, gameLen, snap)) {
            continue;
        }
        ++received;
        if ((received % 20u) == 1u || received == expectTicks) {
            std::fprintf(
                stderr,
                "client: snapshot #%u simTick=%u pos.x=%.4f\n",
                received,
                snap.simTick,
                static_cast<double>(snap.positionLocal.x)
            );
        }
        if (received >= expectTicks) {
            break;
        }
    }

    std::fprintf(stderr, "client: received %u snapshots — ok\n", received);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    bool serverMode = false;
    bool clientMode = false;
    std::uint16_t port = kDefaultPort;
    char const* host = nullptr;
    std::uint32_t snapshotTicks = 120u;
    std::uint32_t expectTicks = 120u;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--server") == 0) {
            serverMode = true;
            continue;
        }
        if (std::strcmp(argv[i], "--client") == 0) {
            clientMode = true;
            continue;
        }
        if (std::strcmp(argv[i], "--host") == 0 && (i + 1) < argc) {
            host = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--port") == 0 && (i + 1) < argc) {
            if (!parseU16(argv[++i], port)) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--snapshot-ticks") == 0 && (i + 1) < argc) {
            char* end{};
            unsigned long const v = std::strtoul(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || v == 0ul || v > 0xffffffful) {
                return usage();
            }
            snapshotTicks = static_cast<std::uint32_t>(v);
            continue;
        }
        if (std::strcmp(argv[i], "--expect-ticks") == 0 && (i + 1) < argc) {
            char* end{};
            unsigned long const v = std::strtoul(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || v == 0ul || v > 0xffffffful) {
                return usage();
            }
            expectTicks = static_cast<std::uint32_t>(v);
            continue;
        }
        return usage();
    }

    if (serverMode == clientMode) {
        return usage();
    }

    if (serverMode) {
        return runServer(port, snapshotTicks);
    }
    if (host == nullptr || host[0] == '\0') {
        return usage();
    }
    return runClient(host, port, expectTicks);
}
