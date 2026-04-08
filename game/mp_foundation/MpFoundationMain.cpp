// Phase 3 sample: AuthoritativeSession + ClientSession over UDP.
// Server binds, initializes the session, and relies on UdpGameTransport Hello demux for peer registration.
// Client uses ClientSession which handles Hello/retry, HelloAck, and snapshot ring buffer.

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
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
        "mp_foundation — Marble Phase 3 transport sample (AuthoritativeSession + ClientSession)\n"
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

    AuthoritativeSession<> session{};
    SessionConfig config{};
    config.mode = MultiplayerMode::DedicatedServer;
    config.maxPlayers = 2u;
    config.simulationHz = 60u;
    config.snapshotHz = 20u;
    if (!session.initialize(config, &transport)) {
        std::fprintf(stderr, "server: session init failed\n");
        return 1;
    }

    auto const bootTime = std::chrono::steady_clock::now();

    // Tick loop — UdpGameTransport demuxes unknown Hello into peer rows; session handles HelloAck and snapshots.
    std::uint32_t const ticksPerSnapshot = config.simulationHz / config.snapshotHz;
    std::uint32_t connectedAtTick = 0u;
    bool wasConnected = false;
    auto lastTime = std::chrono::steady_clock::now();

    for (;;) {
        auto const now = std::chrono::steady_clock::now();
        float const dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        std::uint32_t const tick = session.currentTick();
        static_cast<void>(session.setEntity(0, WorldObjectRef{0x1000u},
            marble::math::Vec3{static_cast<float>(tick) * 0.01f, 0.f, 0.f},
            marble::math::Vec3{1.f, 0.f, 0.f}));

        session.tick(dt);

        if (session.peerCount() == 0u) {
            if (std::chrono::steady_clock::now() - bootTime > std::chrono::seconds(30)) {
                std::fprintf(stderr, "server: handshake timeout (no client in 30s)\n");
                return 1;
            }
        }

        if (!wasConnected && session.peerCount() > 0u) {
            wasConnected = true;
            connectedAtTick = session.currentTick();
            std::fprintf(stderr, "server: client connected at simTick %u\n", connectedAtTick);
        }

        if (wasConnected) {
            std::uint32_t const elapsed = session.currentTick() - connectedAtTick;
            std::uint32_t const approxSent = elapsed / ticksPerSnapshot;
            if (approxSent > 0u && (approxSent % 20u) == 0u &&
                elapsed % ticksPerSnapshot == 0u) {
                std::fprintf(stderr, "server: ~%u snapshots (simTick=%u)\n",
                    approxSent, session.currentTick());
            }
            if (approxSent >= snapshotTicks) {
                break;
            }
        }

        if (std::chrono::steady_clock::now() - bootTime > std::chrono::seconds(60)) {
            std::fprintf(stderr, "server: overall timeout\n");
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::fprintf(stderr, "server: done (simTick=%u)\n", session.currentTick());
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

    ClientSession<> client{};
    if (!client.initialize(&transport, kServerPeerId, 60u, 0xdeadbeefu)) {
        std::fprintf(stderr, "client: session init failed\n");
        return 1;
    }
    std::fprintf(stderr, "client: session initialized, connecting to %s:%u\n",
        host, static_cast<unsigned>(serverPort));

    auto lastTime = std::chrono::steady_clock::now();
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::uint32_t received = 0u;
    std::size_t lastSnapshotCount = 0u;

    while (received < expectTicks) {
        auto const now = std::chrono::steady_clock::now();
        float const dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        client.tick(dt);

        if (client.state() == ConnectionState::Connected) {
            std::size_t const currentCount = client.snapshotCount();
            if (currentCount > lastSnapshotCount) {
                lastSnapshotCount = currentCount;

                EntityKinematicsSnapshot snap{};
                std::size_t const count = client.readLatestEntities(&snap, 1u);
                if (count > 0u) {
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
                }
            }
        }

        if (now > deadline) {
            std::fprintf(stderr, "client: snapshot receive timeout (got %u/%u)\n",
                received, expectTicks);
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    client.disconnect();
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
