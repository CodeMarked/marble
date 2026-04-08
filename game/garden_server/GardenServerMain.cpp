// Headless dedicated server for the garden sample.
// Runs Jolt physics authoritatively and emits entity snapshots to UDP clients.

#include "garden/GardenSimulation.hpp"
#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "gameplay/UdpGameTransport.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

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
using namespace marble::physics;
using marble::math::Vec3;

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
        "  garden_server [--port N] [--seed S] [--max-players N]\n"
        "  Defaults: port %u, seed %u, max-players %u\n",
        static_cast<unsigned>(kDefaultPort),
        static_cast<unsigned>(kDefaultSeed),
        static_cast<unsigned>(kDefaultMaxPlayers)
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

[[nodiscard]] std::size_t gardenServerClientMarbleCount(std::uint16_t maxPlayersRoster) noexcept {
    if (maxPlayersRoster < 2u) {
        return 1u;
    }
    std::size_t const n = static_cast<std::size_t>(maxPlayersRoster);
    return std::min(n, static_cast<std::size_t>(8u));
}

void fillGardenServerMarbleSpawnStates(
    std::span<marble::physics::RigidBodyKinematics> marbles,
    marble::garden::GardenLayout const& layout
) noexcept {
    if (marbles.empty()) {
        return;
    }
    std::size_t const count = marbles.size();
    if (count <= 2u) {
        std::array<marble::physics::RigidBodyKinematics, 2> pair{};
        marble::garden::placeMarblesInArena(pair, layout);
        for (std::size_t i = 0u; i < count; ++i) {
            marbles[i] = pair[i];
        }
        return;
    }
    std::array<marble::physics::RigidBodyKinematics, 2> pair{};
    marble::garden::placeMarblesInArena(pair, layout);
    marbles[0] = pair[0];
    marbles[1] = pair[1];
    float const invMass = 1.f / marble::garden::kPlayerBallMassKg;
    std::size_t const ringCount = count - 2u;
    for (std::size_t i = 2u; i < count; ++i) {
        float const t = 6.2831853f * static_cast<float>(i - 2u) / static_cast<float>(ringCount);
        float const r = marble::garden::kArenaRadius * 0.55f;
        float const x = std::cos(t) * r;
        float const z = std::sin(t) * r;
        float const y = marble::garden::gardenTerrainHeightAt(layout.terrain, x, z) +
            marble::garden::kMarbleRadius + 0.12f;
        marbles[i] = marble::physics::RigidBodyKinematics{{x, y, z}, {}, invMass};
    }
}

} // namespace

int main(int argc, char** argv) {
    std::uint16_t port = kDefaultPort;
    std::uint32_t seed = kDefaultSeed;
    std::uint16_t maxPlayers = kDefaultMaxPlayers;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && (i + 1) < argc) {
            if (!parseU16(argv[++i], port)) {
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
            if (!parseU16(argv[++i], maxPlayers) || maxPlayers < 2u) {
                return usage();
            }
            continue;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            return usage();
        }
        std::fprintf(stderr, "garden_server: unknown argument '%s'\n", argv[i]);
        return usage();
    }

    std::fprintf(stderr, "garden_server: building layout (seed=%u)...\n", seed);
    marble::garden::GardenLayout layout{};
    marble::garden::buildGardenLayout(seed, layout);

    auto physicsScene = createJoltPhysicsScene();

    SimulationIsland island{};
    PhysicsStaticHeightFieldDesc hfDesc{};
    hfDesc.offset = localGameplayToJolt(island, layout.terrain.origin);
    hfDesc.scale = {layout.terrain.cellSize, 1.f, layout.terrain.cellSize};
    hfDesc.sampleCount = layout.terrain.sampleCount;
    hfDesc.heights = std::span<float const>(layout.terrain.heights.data(), layout.terrain.heights.size());
    hfDesc.material.restitution = 0.35f;
    hfDesc.material.friction = 0.7f;
    static_cast<void>(physicsScene->addStaticHeightField(hfDesc));

    for (std::size_t i = 0u; i < layout.staticColliders.size(); ++i) {
        PhysicsStaticBoxDesc boxDesc{};
        boxDesc.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
        boxDesc.material.restitution = 0.25f;
        boxDesc.material.friction = 0.6f;
        static_cast<void>(physicsScene->addStaticBox(boxDesc));
    }

    constexpr std::size_t kMaxMarbles = 8;
    std::array<RigidBodyKinematics, kMaxMarbles> marbles{};
    std::array<PhysicsBodyId, kMaxMarbles> marbleBodyIds{};

    std::size_t const marbleCount = gardenServerClientMarbleCount(maxPlayers);
    fillGardenServerMarbleSpawnStates(std::span<RigidBodyKinematics>(marbles.data(), marbleCount), layout);

    for (std::size_t i = 0u; i < marbleCount; ++i) {
        PhysicsDynamicSphereDesc desc{};
        desc.center = localGameplayToJolt(island, marbles[i].position);
        desc.linearVelocity = marbles[i].linearVelocity;
        desc.radius = marble::garden::kMarbleRadius;
        desc.invMass = 1.f / marble::garden::kPlayerBallMassKg;
        desc.material.restitution = 0.45f;
        desc.material.friction = 0.5f;
        desc.material.linearDamping = 0.04f;
        marbleBodyIds[i] = physicsScene->addDynamicSphere(desc);
    }

    physicsScene->optimizeBroadPhase();
    std::fprintf(stderr, "garden_server: physics scene ready (%zu marbles, %zu static colliders)\n",
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
    config.snapshotHz = 20u;
    std::fprintf(stderr,
        "garden_server: replication sim_hz=%u snapshot_hz=%u max_players=%u (Ctrl+C to stop)\n",
        static_cast<unsigned>(config.simulationHz),
        static_cast<unsigned>(config.snapshotHz),
        static_cast<unsigned>(maxPlayers));

    if (!session.initialize(config, &transport)) {
        std::fprintf(stderr, "garden_server: session init failed\n");
        return 1;
    }

    auto const bootTime = std::chrono::steady_clock::now();
    std::fprintf(stderr, "garden_server: waiting for first client (session ready, send Hello)...\n");

    PhysicsWorldSettings worldSettings{};
    worldSettings.gravity = {0.f, -9.81f, 0.f};
    // Small dynamic count; sleeping spheres that settle in tight contacts can ignore host-applied impulses until
    // re-activated — keep marbles awake for responsive authoritative control.
    worldSettings.enableSleeping = false;
    worldSettings.enableContinuousCollision = true;
    worldSettings.maxSubSteps = 1u;

    constexpr float kFixedDt = 1.f / 60.f;
    auto lastTime = std::chrono::steady_clock::now();
    float accumulator = 0.f;
    std::uint64_t totalTicks = 0u;
    std::array<float, kMaxMarbles> serverPeerJumpHoldSec{};
    std::array<bool, kMaxMarbles> serverPeerJumpWasHeld{};
    bool loggedFirstClient = false;
    constexpr PeerId kClientPeerScanEnd =
        static_cast<PeerId>(2u + marble::gameplay::UdpGameTransport::kMaxPeers);

    while (!gGardenServerQuit.load(std::memory_order_relaxed)) {
        auto const now = std::chrono::steady_clock::now();
        float const dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        accumulator += dt;
        while (accumulator >= kFixedDt) {
            accumulator -= kFixedDt;

            // 1. Process transport (receives client input) + emit snapshots of previous state.
            session.tick(kFixedDt);

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

            // 2. Apply client input impulses before physics step (peer N uses marble slot N-2).
            for (PeerId peer = 2u; peer < kClientPeerScanEnd; ++peer) {
                auto const* inp = session.latestInput(peer);
                if (inp == nullptr) {
                    continue;
                }
                if (!session.isConnected(peer)) {
                    session.clearInput(peer);
                    continue;
                }
                std::size_t const slot = static_cast<std::size_t>(peer) - 2u;
                if (slot >= marbleCount) {
                    session.clearInput(peer);
                    continue;
                }

                bool const jumpHeld = (inp->buttons & kClientInputButton_Jump) != 0;
                constexpr float kMarbleRoll = 4.6f;
                constexpr float kMaxHoriz = 5.2f;

                float const mx = inp->moveX;
                float const mz = inp->moveZ;
                float const mag = std::sqrt(mx * mx + mz * mz);
                if (mag > 1e-5f && marbles[slot].invMass > 0.f) {
                    float const nx = mx / mag;
                    float const nz = mz / mag;
                    Vec3 const wish{nx, 0.f, nz};
                    applyImpulseLinear(marbles[slot], wish * (kMarbleRoll * kFixedDt));

                    float const vx = marbles[slot].linearVelocity.x;
                    float const vz = marbles[slot].linearVelocity.z;
                    float const vh = std::sqrt(vx * vx + vz * vz);
                    if (vh > kMaxHoriz && vh > 1e-6f) {
                        float const s = kMaxHoriz / vh;
                        marbles[slot].linearVelocity.x *= s;
                        marbles[slot].linearVelocity.z *= s;
                    }
                }

                if (jumpHeld) {
                    serverPeerJumpHoldSec[slot] += kFixedDt;
                    serverPeerJumpHoldSec[slot] = std::min(
                        serverPeerJumpHoldSec[slot], marble::garden::kGardenJumpChargeMaxSec);
                } else {
                    if (serverPeerJumpWasHeld[slot] && marbles[slot].invMass > 0.f &&
                        serverPeerJumpHoldSec[slot] > 1e-4f &&
                        marble::garden::gardenBallOnGround(
                            layout, marbles[slot], marble::garden::kMarbleRadius)) {
                        float const imp = marble::garden::gardenJumpImpulseFromHoldSeconds(
                            serverPeerJumpHoldSec[slot]);
                        applyImpulseLinear(marbles[slot], Vec3{0.f, imp, 0.f});
                    }
                    serverPeerJumpHoldSec[slot] = 0.f;
                }
                serverPeerJumpWasHeld[slot] = jumpHeld;

                session.clearInput(peer);
            }

            // 3. Physics step: sync host velocities -> Jolt step -> read back.
            std::span<PhysicsBodyId const> bodySpan(marbleBodyIds.data(), marbleCount);
            physicsScene->syncHostVelocitiesBeforeStep(bodySpan, marbles.data(), marbleCount);

            PhysicsCylindricalXZClamp clamp{};
            clamp.maxHorizontalRadiusFromYAxis = marble::garden::kGardenRadius;
            clamp.minCenterY = -5.f;
            PhysicsStepOptions stepOpts{};
            stepOpts.postStepCylindricalClamp = &clamp;
            stepOpts.clampBodyIds = bodySpan;

            physicsScene->step(kFixedDt, worldSettings, stepOpts);
            physicsScene->readBackKinematics(bodySpan, marbles.data(), marbleCount);

            // 4. Update session entities with post-physics state.
            for (std::size_t i = 0u; i < marbleCount; ++i) {
                static_cast<void>(session.setEntity(
                    i,
                    WorldObjectRef{static_cast<std::uint64_t>(0x1000u + i)},
                    marbles[i].position,
                    marbles[i].linearVelocity
                ));
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
