#pragma once

#include "garden/GardenSimulation.hpp"

#include "gameplay/ClientSession.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/SnapshotInterpolator.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace marble::garden_app {

/// Single load of optional `GARDEN_REMOTE_*` overrides (defaults match [`detail/GardenGameState.hpp`] constants).
struct GardenRemoteNetTuning {
    std::uint32_t interpDelayMinTicks{};
    std::uint32_t interpDelayMaxTicks{};
    float clampMinSpeedMps{};
    float clampDvMps{};
    std::uint32_t maxExtrapolationTicks{};
    float velocityJumpBlendMps{};
    float velocityJumpExtrapMps{};
    bool noPredict{};
    bool desyncLog{};
    int desyncLogPeriodMs{};
    bool interpLog{};
    int interpLogPeriodMs{};
    float interpRttScale{};
    std::uint32_t interpDelaySmoothMaxTicks{};
};

[[nodiscard]] GardenRemoteNetTuning gardenRemoteNetTuningFromEnv() noexcept;

void gardenRemoteApplyInterpTunables(
    marble::gameplay::SnapshotInterpolator<>& interp,
    GardenRemoteNetTuning const& tuning) noexcept;

/// Sample [`SnapshotInterpolator`] at presentation time and write **all** marble slots from network state
/// (plus optional latest-clamp for fast extrapolated remotes).
void gardenRemoteRunInterpolationPhase(
    GardenRemoteNetTuning const& tuning,
    marble::gameplay::ClientSession<>& clientSession,
    marble::gameplay::SnapshotInterpolator<>& interp,
    float serverSimulationHz,
    std::span<marble::physics::RigidBodyKinematics> marbles,
    std::size_t marbleCount,
    std::span<float> remoteDisplayYaw,
    std::size_t localViewSlot,
    std::uint32_t& interpDelaySmoothedTicks,
    std::span<marble::gameplay::PhysicsSimulationTier> outMarbleReplicationTier,
    std::optional<std::chrono::steady_clock::time_point>& lastInterpLogTime) noexcept;

/// Integrate local-player **horizontal** prediction at **fixedPhysicsDt** cadence using `wallDeltaSeconds`
/// (variable frame time), then reconcile vs last authoritative sample.
void gardenRemoteRunPredictReconcileAccum(
    GardenRemoteNetTuning const& tuning,
    float wallDeltaSeconds,
    float fixedPhysicsDt,
    float& fixedAccumInOut,
    std::size_t localSlot,
    marble::garden::GardenLayout const& layout,
    float pendingMoveX,
    float pendingMoveZ,
    std::uint8_t pendingButtons,
    bool& jumpWasHeld,
    float& jumpChargeSec,
    std::span<marble::physics::RigidBodyKinematics> marbles,
    std::span<marble::math::Vec3 const> remoteAuthPos,
    std::span<bool const> remoteAuthValid,
    marble::gameplay::ClientSession<>& clientSession,
    marble::gameplay::SnapshotInterpolator<> const* interpDiag,
    float serverSimulationHz,
    std::uint32_t lastAckedServerSimTick,
    std::uint64_t& desyncHardSnapCount,
    std::optional<std::chrono::steady_clock::time_point>& lastDesyncLogTime) noexcept;

} // namespace marble::garden_app
