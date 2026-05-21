// Headless dedicated server for the garden sample.
// Runs Jolt physics authoritatively and emits entity snapshots to UDP clients.

#include "garden/GardenAuthorityTick.hpp"
#include "garden/GardenSimulation.hpp"
#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "gameplay/UdpGameTransport.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/RigidBodyDynamics.hpp"
#include "platform/cli/CmdlineU.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <memory>
#include <span>
#include <thread>

namespace {

using namespace marble::gameplay;
using marble::physics::PhysicsBodyId;
using marble::physics::PhysicsDynamicSphereDesc;
using marble::physics::RigidBodyKinematics;
using marble::physics::createJoltPhysicsScene;

std::atomic<bool> gGardenServerQuit{false};

void gardenServerSignalHandler(int signal) noexcept {
    (void)signal;
    gGardenServerQuit.store(true, std::memory_order_relaxed);
}

inline constexpr std::uint16_t kDefaultPort = 27778u;
inline constexpr std::uint32_t kDefaultSeed = marble::garden::kGardenDedicatedServerDefaultLayoutSeed;
inline constexpr std::uint16_t kDefaultMaxPlayers = 4u;

[[nodiscard]] int usage() {
    std::fprintf(stderr,
        "garden_server — Marble headless dedicated garden server\n"
        "  garden_server [--port N] [--seed S] [--max-players N] [--snapshot-hz N] [--aoi] [--aoi-radius R] [--no-aoi]\n"
        "                [--aoi-lookahead SEC] [--aoi-viewer-lookahead SEC] [--snapshot-max-bytes N]\n"
        "                [--join-token V]  (decimal or 0x hex; or env GARDEN_SERVER_JOIN_TOKEN)\n"
        "  Defaults: port %u, seed %u, max-players %u, snapshot-hz 20 (must divide sim 60 Hz; AOI off; snapshot bytes 1400)\n",
        static_cast<unsigned>(kDefaultPort),
        static_cast<unsigned>(kDefaultSeed),
        static_cast<unsigned>(kDefaultMaxPlayers)
    );
    return 2;
}

[[nodiscard]] bool parseF32(char const* s, float& out) {
    if (s == nullptr || s[0] == '\0') {
        return false;
    }
    char* end{};
    float const v = std::strtof(s, &end);
    if (end == s || *end != '\0' || !(v > 0.f) || v > 1.0e7f) {
        return false;
    }
    out = v;
    return true;
}

[[nodiscard]] bool parseF32NonNegative(char const* s, float& out) {
    if (s == nullptr || s[0] == '\0') {
        return false;
    }
    char* end{};
    float const v = std::strtof(s, &end);
    if (end == s || *end != '\0' || !std::isfinite(v) || v < 0.f || v > 1.0e3f) {
        return false;
    }
    out = v;
    return true;
}

[[nodiscard]] bool parseU32(char const* s, std::uint32_t& out) {
    if (s == nullptr || s[0] == '\0') {
        return false;
    }
    char* end{};
    unsigned long const v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0') {
        return false;
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

[[nodiscard]] bool parseU32Flexible(char const* s, std::uint32_t& out) {
    if (s == nullptr || s[0] == '\0') {
        return false;
    }
    char* end{};
    unsigned long const v = std::strtoul(s, &end, 0);
    if (end == s || *end != '\0' || v > 4294967295ul) {
        return false;
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::uint16_t port = kDefaultPort;
    std::uint32_t seed = kDefaultSeed;
    std::uint16_t maxPlayers = kDefaultMaxPlayers;
    float aoiRadius = 2500.f;
    bool useAoi = false;
    float aoiEntityLookaheadSec = 0.5f;
    float aoiViewerLookaheadSec = 0.f;
    std::uint32_t snapshotMaxBytes = 1400u;
    std::uint16_t snapshotHz = 20u;
    std::uint32_t joinTokenU32 = 0u;
    bool joinTokenFromArgv = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && (i + 1) < argc) {
            if (!marble::platform::cli::parseU16(argv[++i], port)) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--seed") == 0 && (i + 1) < argc) {
            std::uint32_t s{};
            if (!parseU32(argv[++i], s)) {
                return usage();
            }
            seed = s;
            continue;
        }
        if (std::strcmp(argv[i], "--max-players") == 0 && (i + 1) < argc) {
            if (!marble::platform::cli::parseU16(argv[++i], maxPlayers) || maxPlayers < 2u) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--aoi-radius") == 0 && (i + 1) < argc) {
            if (!parseF32(argv[++i], aoiRadius)) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--aoi") == 0) {
            useAoi = true;
            continue;
        }
        if (std::strcmp(argv[i], "--no-aoi") == 0) {
            useAoi = false;
            continue;
        }
        if (std::strcmp(argv[i], "--aoi-lookahead") == 0 && (i + 1) < argc) {
            if (!parseF32NonNegative(argv[++i], aoiEntityLookaheadSec)) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--aoi-viewer-lookahead") == 0 && (i + 1) < argc) {
            if (!parseF32NonNegative(argv[++i], aoiViewerLookaheadSec)) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--snapshot-max-bytes") == 0 && (i + 1) < argc) {
            if (!parseU32(argv[++i], snapshotMaxBytes)) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--snapshot-hz") == 0 && (i + 1) < argc) {
            if (!marble::platform::cli::parseU16(argv[++i], snapshotHz) || snapshotHz < 1u || snapshotHz > 60u) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--join-token") == 0 && (i + 1) < argc) {
            if (!parseU32Flexible(argv[++i], joinTokenU32)) {
                return usage();
            }
            joinTokenFromArgv = true;
            continue;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            return usage();
        }
        std::fprintf(stderr, "garden_server: unknown argument '%s'\n", argv[i]);
        return usage();
    }

    if (!joinTokenFromArgv) {
        if (char const* envTok = std::getenv("GARDEN_SERVER_JOIN_TOKEN")) {
            static_cast<void>(parseU32Flexible(envTok, joinTokenU32));
        }
    }

    {
        SessionConfig probe{};
        probe.mode = MultiplayerMode::DedicatedServer;
        probe.maxPlayers = maxPlayers;
        probe.simulationHz = 60u;
        probe.snapshotHz = snapshotHz;
        probe.maxPredictionTicks = 2u;
        probe.joinTokenU32 = joinTokenU32;
        probe.maxSnapshotBytesPerPeer = snapshotMaxBytes;
        if (!isValid(probe)) {
            std::fprintf(
                stderr,
                "garden_server: invalid --snapshot-hz %u (must be 1..60 and divide simulation rate 60 evenly; "
                "e.g. 10, 12, 15, 20, 30)\n",
                static_cast<unsigned>(snapshotHz));
            return 2;
        }
    }

    std::fprintf(stderr, "garden_server: building layout (seed=%u)...\n", seed);
    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(seed, layout);

    auto physicsScene = createJoltPhysicsScene();

    SimulationIsland island{};
    marble::garden::gardenAuthorityPopulateStaticCollidersFromLayout(*physicsScene, layout, island);

    std::size_t constexpr kMaxMarbles = marble::garden::kMaxGardenAuthorityMarbles;
    std::array<RigidBodyKinematics, kMaxMarbles> marbles{};
    std::array<PhysicsBodyId, kMaxMarbles> marbleBodyIds{};

    std::size_t const marbleCount = marble::garden::gardenServerClientMarbleCount(maxPlayers);
    marble::garden::fillGardenServerMarbleSpawnStates(std::span<RigidBodyKinematics>(marbles.data(), marbleCount), layout);

    for (std::size_t i = 0u; i < marbleCount; ++i) {
        PhysicsDynamicSphereDesc desc{};
        desc.center = localGameplayToJolt(island, marbles[i].position);
        desc.linearVelocity = marbles[i].linearVelocity;
        desc.radius = marble::garden::kMarbleRadius;
        desc.invMass = 1.f / marble::garden::kPlayerBallMassKg;
        desc.material.restitution = 0.672f;
        desc.material.friction = 0.42f;
        desc.material.linearDamping = 0.02f;
        desc.material.angularDamping = 0.10f;
        desc.enhancedInternalEdgeRemoval = true;
        marbleBodyIds[i] = physicsScene->addDynamicSphere(desc);
    }

    physicsScene->optimizeBroadPhase();
    std::fprintf(stderr, "garden_server: physics scene ready (%zu player marbles, %zu static colliders)\n",
        marbleCount, layout.staticColliders.size());

    UdpGameTransport transport{};
    if (!transport.bind(port)) {
        std::fprintf(stderr, "garden_server: failed to bind UDP port %u\n", static_cast<unsigned>(port));
        return 1;
    }
    std::fprintf(stderr, "garden_server: listening on UDP port %u\n", static_cast<unsigned>(transport.localPort()));

    static_cast<void>(std::signal(SIGINT, gardenServerSignalHandler));
#if defined(_WIN32)
    static_cast<void>(std::signal(SIGBREAK, gardenServerSignalHandler));
#else
    static_cast<void>(std::signal(SIGTERM, gardenServerSignalHandler));
#endif

    AuthoritativeSession<> session{};
    SessionConfig config{};
    config.mode = MultiplayerMode::DedicatedServer;
    config.maxPlayers = maxPlayers;
    config.simulationHz = 60u;
    config.snapshotHz = snapshotHz;
    config.joinTokenU32 = joinTokenU32;
    config.maxSnapshotBytesPerPeer = snapshotMaxBytes;
    std::fprintf(stderr,
        "garden_server: replication sim_hz=%u snapshot_hz=%u max_players=%u (Ctrl+C to stop)\n",
        static_cast<unsigned>(config.simulationHz),
        static_cast<unsigned>(config.snapshotHz),
        static_cast<unsigned>(maxPlayers));
    if (joinTokenU32 != 0u) {
        std::fprintf(stderr, "garden_server: join token required (non-zero)\n");
    }

    if (!session.initialize(config, &transport)) {
        std::fprintf(stderr, "garden_server: session init failed\n");
        return 1;
    }

    if (useAoi) {
        session.setAoiEnabled(true);
        session.setDefaultAoiRadius(aoiRadius);
        session.setAoiEntityVelocityLookaheadSeconds(aoiEntityLookaheadSec);
        session.setAoiViewerPositionLookaheadSeconds(aoiViewerLookaheadSec);
        std::fprintf(stderr,
            "garden_server: AOI enabled (radius=%.1f entity_lookahead_s=%.3f viewer_lookahead_s=%.3f)\n",
            static_cast<double>(aoiRadius),
            static_cast<double>(aoiEntityLookaheadSec),
            static_cast<double>(aoiViewerLookaheadSec));
    } else {
        session.setAoiEnabled(false);
        std::fprintf(stderr, "garden_server: AOI disabled (full snapshots per peer)\n");
    }
    std::fprintf(stderr,
        "garden_server: max_snapshot_bytes_per_peer=%u (0=unlimited)\n",
        static_cast<unsigned>(snapshotMaxBytes));

    session.setFullSnapshotWhenActiveEntityCountAtMost(
        static_cast<std::uint32_t>(marble::garden::kMaxGardenAuthorityMarbles));
    std::fprintf(stderr,
        "garden_server: full kinematic snapshot when active entities <= %u (garden roster)\n",
        static_cast<unsigned>(marble::garden::kMaxGardenAuthorityMarbles));

    auto const bootTime = std::chrono::steady_clock::now();
    std::fprintf(stderr, "garden_server: waiting for first client (session ready, send Hello)...\n");

    marble::physics::PhysicsWorldSettings const worldSettings = marble::garden::gardenAuthorityPhysicsWorldSettings();

    auto lastTime = std::chrono::steady_clock::now();
    float accumulator = 0.f;
    std::uint64_t totalTicks = 0u;
    std::array<float, kMaxMarbles> serverPeerJumpHoldSec{};
    std::array<bool, kMaxMarbles> serverPeerJumpWasHeld{};
    bool loggedFirstClient = false;

    while (!gGardenServerQuit.load(std::memory_order_relaxed)) {
        auto const now = std::chrono::steady_clock::now();
        float const dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        accumulator += dt;
        while (accumulator >= marble::garden::kGardenAuthorityFixedDeltaSeconds) {
            accumulator -= marble::garden::kGardenAuthorityFixedDeltaSeconds;

            marble::garden::gardenAuthorityFixedStep(
                marble::garden::GardenAuthorityRunMode::Dedicated,
                session,
                marble::garden::kGardenAuthorityFixedDeltaSeconds,
                useAoi,
                *physicsScene,
                layout,
                std::span<RigidBodyKinematics>(marbles.data(), marbleCount),
                std::span<PhysicsBodyId const>(marbleBodyIds.data(), marbleCount),
                marbleCount,
                std::span<float>(serverPeerJumpHoldSec.data(), marbleCount),
                std::span<bool>(serverPeerJumpWasHeld.data(), marbleCount),
                worldSettings);

            if (session.peerCount() == 0u) {
                if (gGardenServerQuit.load(std::memory_order_relaxed)) {
                    std::fprintf(stderr, "garden_server: shutdown before first client connected\n");
                    return 0;
                }
                if (std::chrono::steady_clock::now() - bootTime > std::chrono::seconds(300)) {
                    std::fprintf(stderr, "garden_server: no client after 5 minutes, exiting\n");
                    return 0;
                }
                continue;
            }

            if (!loggedFirstClient) {
                loggedFirstClient = true;
                std::fprintf(stderr, "garden_server: first client connected (%zu peer(s))\n",
                    session.peerCount());
            }

            ++totalTicks;

            if (totalTicks % 600u == 0u) {
                std::fprintf(stderr, "garden_server: tick %llu, %zu peers connected\n",
                    static_cast<unsigned long long>(totalTicks), session.peerCount());
            }
        }

        auto const elapsed = std::chrono::steady_clock::now() - now;
        auto const sleepMs = std::chrono::milliseconds(1) - elapsed;
        if (sleepMs.count() > 0) {
            std::this_thread::sleep_for(sleepMs);
        }
    }

    std::fprintf(stderr, "garden_server: shutdown (ticks=%llu)\n",
        static_cast<unsigned long long>(totalTicks));
    return 0;
}
