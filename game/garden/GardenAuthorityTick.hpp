#pragma once

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/UdpGameTransport.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "physics/MiddlewarePhysicsTypes.hpp"

namespace marble::physics {
class IPhysicsScene;
struct RigidBodyKinematics;
struct PhysicsWorldSettings;
} // namespace marble::physics

namespace marble::garden {
struct GardenLayout;
}

namespace marble::gameplay {
struct SimulationIsland;
}

namespace marble::garden {

inline constexpr std::size_t kMaxGardenAuthorityMarbles = 8u;

enum class GardenAuthorityRunMode { Dedicated, ListenHost };

[[nodiscard]] std::size_t gardenServerClientMarbleCount(std::uint16_t maxPlayersRoster) noexcept;

void fillGardenServerMarbleSpawnStates(
    std::span<marble::physics::RigidBodyKinematics> marbles,
    GardenLayout const& layout) noexcept;

/// Static terrain + props; matches dedicated [`garden_server`](../garden_server/GardenServerMain.cpp) setup.
void gardenAuthorityPopulateStaticCollidersFromLayout(
    marble::physics::IPhysicsScene& scene,
    GardenLayout const& layout,
    marble::gameplay::SimulationIsland const& island);

/// One authoritative simulation step: session transport + snapshots, optional AOI view hooks, inputs, Jolt,
/// replicated entities. [`GardenAuthorityRunMode::Dedicated`] skips physics when no UDP peers are connected.
void gardenAuthorityFixedStep(
    GardenAuthorityRunMode mode,
    marble::gameplay::AuthoritativeSession<>& session,
    float fixedDt,
    bool useAoi,
    marble::physics::IPhysicsScene& physicsScene,
    GardenLayout const& layout,
    std::span<marble::physics::RigidBodyKinematics> marbles,
    std::span<marble::physics::PhysicsBodyId const> marbleBodyIds,
    std::size_t marbleCount,
    std::span<float> jumpHoldSeconds,
    std::span<bool> jumpWasHeld,
    marble::physics::PhysicsWorldSettings const& worldSettings);

} // namespace marble::garden
