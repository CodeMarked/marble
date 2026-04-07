// Headless dedicated server for the garden sample.
// Runs Jolt physics authoritatively and emits entity snapshots to UDP clients.

#include "garden/GardenSimulation.hpp"
#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "gameplay/UdpGameTransport.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

namespace {

using namespace marble::gameplay;
using namespace marble::physics;
using marble::math::Vec3;

inline constexpr PeerId kServerPeerId = 1u;
inline constexpr std::uint16_t kDefaultPort = 27778u;
inline constexpr std::uint32_t kDefaultSeed = 42u;
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
    physicsScene->addStaticHeightField(hfDesc);

    for (std::size_t i = 0u; i < layout.staticColliders.size(); ++i) {
        PhysicsStaticBoxDesc boxDesc{};
        boxDesc.bounds = localGameplayAabbToJolt(island, layout.staticColliders[i]);
        boxDesc.material.restitution = 0.25f;
        boxDesc.material.friction = 0.6f;
        physicsScene->addStaticBox(boxDesc);
    }

    constexpr std::size_t kMaxMarbles = 8;
    std::array<RigidBodyKinematics, kMaxMarbles> marbles{};
    std::array<PhysicsBodyId, kMaxMarbles> marbleBodyIds{};

    std::array<RigidBodyKinematics, 2> spawnPoses{};
    marble::garden::placeMarblesInArena(spawnPoses, layout);

    std::size_t marbleCount = 2u;
    for (std::size_t i = 0u; i < marbleCount; ++i) {
        marbles[i] = spawnPoses[i];
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

    AuthoritativeSession<> session{};
    SessionConfig config{};
    config.mode = MultiplayerMode::DedicatedServer;
    config.maxPlayers = maxPlayers;
    config.simulationHz = 60u;
    config.snapshotHz = 20u;

    auto const bootTime = std::chrono::steady_clock::now();
    std::array<std::uint8_t, 2048> rxBuf{};

    std::fprintf(stderr, "garden_server: waiting for first client...\n");
    bool sessionInitialized = false;

    PhysicsWorldSettings worldSettings{};
    worldSettings.gravity = {0.f, -9.81f, 0.f};
    worldSettings.enableSleeping = true;
    worldSettings.enableContinuousCollision = true;
    worldSettings.maxSubSteps = 1u;

    constexpr float kFixedDt = 1.f / 60.f;
    auto lastTime = std::chrono::steady_clock::now();
    float accumulator = 0.f;
    std::uint64_t totalTicks = 0u;

    for (;;) {
        auto const now = std::chrono::steady_clock::now();
        float const dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        if (!sessionInitialized) {
            std::uint32_t srcIp{};
            std::uint16_t srcPort{};
            std::size_t const n = transport.receiveRaw(srcIp, srcPort, rxBuf.data(), rxBuf.size());
            if (n > 0u) {
                SessionMessageType msgType{};
                std::uint8_t flags{};
                std::uint8_t const* payload{};
                std::size_t payloadLen{};
                if (parseSessionEnvelopeEx(rxBuf.data(), n, msgType, flags, payload, payloadLen) &&
                    msgType == SessionMessageType::Hello) {
                    PeerId const clientId = 2u;
                    if (transport.addPeerEndpoint(clientId, srcIp, srcPort)) {
                        if (session.initialize(config, &transport)) {
                            sessionInitialized = true;
                            std::fprintf(stderr, "garden_server: session initialized, client discovered\n");
                        }
                    }
                }
            }

            if (!sessionInitialized) {
                if (std::chrono::steady_clock::now() - bootTime > std::chrono::seconds(300)) {
                    std::fprintf(stderr, "garden_server: no client after 5 minutes, exiting\n");
                    return 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        }

        accumulator += dt;
        while (accumulator >= kFixedDt) {
            accumulator -= kFixedDt;

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

            for (std::size_t i = 0u; i < marbleCount; ++i) {
                static_cast<void>(session.setEntity(
                    i,
                    WorldObjectRef{static_cast<std::uint64_t>(0x1000u + i)},
                    marbles[i].position,
                    marbles[i].linearVelocity
                ));
            }

            session.tick(kFixedDt);
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
}
