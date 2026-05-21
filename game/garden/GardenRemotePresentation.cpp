#include "garden/GardenRemotePresentation.hpp"

#include "garden/GardenMarblePlayer.hpp"
#include "garden/GardenRemoteReconcile.hpp"
#include "garden/detail/GardenGameState.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace marble::garden_app {

using namespace marble::gameplay;
using namespace marble::physics;
using namespace marble::garden;
using marble::math::Vec3;

namespace {

[[nodiscard]] bool envTruthy(char const* name) noexcept {
    char const* v = std::getenv(name);
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

[[nodiscard]] int envDesyncLogPeriodMs() noexcept {
    char const* v = std::getenv("GARDEN_REMOTE_DESYNC_LOG_MS");
    if (v == nullptr || v[0] == '\0') {
        return 500;
    }
    char* end{};
    long const n = std::strtol(v, &end, 10);
    if (end == v || *end != '\0' || n < 50 || n > 60000) {
        return 500;
    }
    return static_cast<int>(n);
}

[[nodiscard]] int envInterpLogPeriodMs() noexcept {
    char const* v = std::getenv("GARDEN_REMOTE_INTERP_LOG_MS");
    if (v == nullptr || v[0] == '\0') {
        return 500;
    }
    char* end{};
    long const n = std::strtol(v, &end, 10);
    if (end == v || *end != '\0' || n < 50 || n > 60000) {
        return 500;
    }
    return static_cast<int>(n);
}

[[nodiscard]] std::uint32_t envU32(char const* name, std::uint32_t defaultV, std::uint32_t minV, std::uint32_t maxV) noexcept {
    char const* v = std::getenv(name);
    if (v == nullptr || v[0] == '\0') {
        return defaultV;
    }
    char* end{};
    unsigned long const n = std::strtoul(v, &end, 10);
    if (end == v || *end != '\0') {
        return defaultV;
    }
    std::uint32_t const u = static_cast<std::uint32_t>(n);
    if (u < minV) {
        return minV;
    }
    if (u > maxV) {
        return maxV;
    }
    return u;
}

[[nodiscard]] float envF32(char const* name, float defaultV, float minV, float maxV) noexcept {
    char const* v = std::getenv(name);
    if (v == nullptr || v[0] == '\0') {
        return defaultV;
    }
    char* end{};
    float const f = std::strtof(v, &end);
    if (end == v || *end != '\0' || !std::isfinite(f)) {
        return defaultV;
    }
    if (f < minV) {
        return minV;
    }
    if (f > maxV) {
        return maxV;
    }
    return f;
}

} // namespace

GardenRemoteNetTuning gardenRemoteNetTuningFromEnv() noexcept {
    GardenRemoteNetTuning t{};
    t.interpDelayMinTicks = envU32(
        "GARDEN_REMOTE_INTERP_DELAY_MIN_TICKS", kGardenRemoteInterpDelayMinTicks, 1u, 30u);
    t.interpDelayMaxTicks = envU32(
        "GARDEN_REMOTE_INTERP_DELAY_MAX_TICKS", kGardenRemoteInterpDelayMaxTicks, 1u, 30u);
    if (t.interpDelayMaxTicks < t.interpDelayMinTicks) {
        t.interpDelayMaxTicks = t.interpDelayMinTicks;
    }
    t.clampMinSpeedMps =
        envF32("GARDEN_REMOTE_CLAMP_MIN_SPEED", kGardenRemoteLatestClampMinSpeedMps, 0.f, 100.f);
    t.clampDvMps = envF32("GARDEN_REMOTE_CLAMP_DV", kGardenRemoteLatestClampDvMps, 0.f, 200.f);
    t.maxExtrapolationTicks =
        envU32("GARDEN_REMOTE_MAX_EXTRAP_TICKS", kGardenRemoteMaxExtrapolationTicks, 0u, 20u);
    t.velocityJumpBlendMps =
        envF32("GARDEN_REMOTE_VELOCITY_JUMP_BLEND_MPS", kGardenRemoteVelocityJumpBlendMps, 0.f, 200.f);
    t.velocityJumpExtrapMps =
        envF32("GARDEN_REMOTE_VELOCITY_JUMP_EXTRAP_MPS", kGardenRemoteVelocityJumpExtrapMps, 0.f, 200.f);
    t.noPredict = envTruthy("GARDEN_REMOTE_NO_PREDICT");
    t.desyncLog = envTruthy("GARDEN_REMOTE_DESYNC_LOG");
    t.desyncLogPeriodMs = envDesyncLogPeriodMs();
    t.interpLog = envTruthy("GARDEN_REMOTE_INTERP_LOG");
    t.interpLogPeriodMs = envInterpLogPeriodMs();
    t.interpRttScale =
        envF32("GARDEN_REMOTE_INTERP_RTT_SCALE", kGardenRemoteInterpRttScale, 0.05f, 1.5f);
    t.interpDelaySmoothMaxTicks = envU32(
        "GARDEN_REMOTE_INTERP_DELAY_SMOOTH_MAX_TICKS",
        kGardenRemoteInterpDelaySmoothMaxTicks,
        1u,
        10u);
    return t;
}

void gardenRemoteApplyInterpTunables(SnapshotInterpolator<>& interp, GardenRemoteNetTuning const& tuning) noexcept {
    interp.setSimulationHz(kGardenRemoteServerSimulationHz);
    interp.setMaxExtrapolationTicks(tuning.maxExtrapolationTicks);
    interp.setVelocityJumpBlendThresholdMps(tuning.velocityJumpBlendMps);
    interp.setVelocityJumpExtrapolationThresholdMps(tuning.velocityJumpExtrapMps);
}

void gardenRemoteRunInterpolationPhase(
    GardenRemoteNetTuning const& tuning,
    ClientSession<>& clientSession,
    SnapshotInterpolator<>& interp,
    float serverSimulationHz,
    std::span<RigidBodyKinematics> marbles,
    std::size_t marbleCount,
    std::span<float> remoteDisplayYaw,
    std::size_t localViewSlot,
    std::uint32_t& interpDelaySmoothedTicks,
    std::span<PhysicsSimulationTier> outMarbleReplicationTier,
    std::optional<std::chrono::steady_clock::time_point>& lastInterpLogTime) noexcept {
    if (clientSession.state() != ConnectionState::Connected || !clientSession.hasServerTimeSync() ||
        interp.frameCount() == 0u) {
        return;
    }

    gardenRemoteApplyInterpTunables(interp, tuning);

    std::uint32_t delayMin = tuning.interpDelayMinTicks;
    std::uint32_t delayMax = tuning.interpDelayMaxTicks;
    float const rtt = clientSession.estimatedRttSeconds();
    std::uint32_t wantDelayTicks = delayMin;
    if (rtt > 1e-5f) {
        std::uint32_t const delayTicks =
            static_cast<std::uint32_t>(rtt * tuning.interpRttScale * serverSimulationHz);
        wantDelayTicks = std::max(delayMin, std::min(delayMax, delayTicks));
    }
    std::uint32_t const smoothStep = tuning.interpDelaySmoothMaxTicks;
    if (interpDelaySmoothedTicks == 0u) {
        interpDelaySmoothedTicks = wantDelayTicks;
    } else if (wantDelayTicks > interpDelaySmoothedTicks) {
        interpDelaySmoothedTicks = std::min(wantDelayTicks, interpDelaySmoothedTicks + smoothStep);
    } else if (wantDelayTicks < interpDelaySmoothedTicks) {
        interpDelaySmoothedTicks = std::max(wantDelayTicks, interpDelaySmoothedTicks - smoothStep);
    } else {
        interpDelaySmoothedTicks = wantDelayTicks;
    }
    interp.setRenderDelayTicks(interpDelaySmoothedTicks);

    float const targetTickF =
        clientSession.estimatedServerSimTickAtNow(serverSimulationHz) -
        static_cast<float>(interp.renderDelayTicks());

    std::array<InterpolatedEntity, 16> interpolated{};
    std::size_t const interpCount = interp.interpolate(targetTickF, interpolated.data(), interpolated.size());

    for (float& y : remoteDisplayYaw) {
        y = 0.f;
    }

    if (!outMarbleReplicationTier.empty()) {
        std::size_t const nTier = std::min(marbleCount, outMarbleReplicationTier.size());
        for (std::size_t i = 0u; i < nTier; ++i) {
            outMarbleReplicationTier[i] = PhysicsSimulationTier::Contact;
        }
    }

    EntityKinematicsSnapshot latestRaw{};
    for (std::size_t i = 0u; i < interpCount; ++i) {
        std::uint64_t const g = interpolated[i].entity.guid;
        if (g < 0x1000u) {
            continue;
        }
        std::size_t const idx = static_cast<std::size_t>(g - 0x1000u);
        if (idx < marbleCount) {
            marbles[idx].position = interpolated[i].position;
            marbles[idx].linearVelocity = interpolated[i].velocity;
        }
        if (idx < remoteDisplayYaw.size()) {
            remoteDisplayYaw[idx] = interpolated[i].yawRadians;
        }
        if (idx < outMarbleReplicationTier.size()) {
            outMarbleReplicationTier[idx] = interpolated[i].tier;
        }
    }

    std::uint32_t extrapRemoteCount = 0u;
    for (std::size_t i = 0u; i < interpCount; ++i) {
        std::uint64_t const g = interpolated[i].entity.guid;
        if (g < 0x1000u) {
            continue;
        }
        std::size_t const idx = static_cast<std::size_t>(g - 0x1000u);
        if (idx < marbleCount && idx != localViewSlot && interpolated[i].extrapolated) {
            ++extrapRemoteCount;
        }
    }

    std::uint32_t latestClampCount = 0u;
    for (std::size_t i = 0u; i < interpCount; ++i) {
        std::uint64_t const g = interpolated[i].entity.guid;
        if (g < 0x1000u) {
            continue;
        }
        std::size_t const idx = static_cast<std::size_t>(g - 0x1000u);
        if (idx >= marbleCount || idx == localViewSlot || !interpolated[i].extrapolated) {
            continue;
        }
        // Prototype: skip aggressive "latest" snap for [`CruiseOrbit`] peers (far / low-fidelity replication tier).
        if (interpolated[i].tier == PhysicsSimulationTier::CruiseOrbit) {
            continue;
        }
        float dvBetweenFrames{};
        bool const hasDv = interp.tryVelocityDeltaBetweenLastTwoFrames(interpolated[i].entity, dvBetweenFrames);
        float const speed = marble::math::length(interpolated[i].velocity);
        bool const needLatestClamp =
            speed >= tuning.clampMinSpeedMps || (hasDv && dvBetweenFrames >= tuning.clampDvMps);
        if (!needLatestClamp) {
            continue;
        }
        if (interp.tryLatestKinematics(interpolated[i].entity, latestRaw)) {
            marbles[idx].position = latestRaw.positionLocal;
            marbles[idx].linearVelocity = latestRaw.linearVelocity;
            if (idx < remoteDisplayYaw.size()) {
                remoteDisplayYaw[idx] = latestRaw.yawRadians;
            }
            ++latestClampCount;
        }
    }

    if (tuning.interpLog) {
        auto const now = std::chrono::steady_clock::now();
        bool shouldLog = false;
        if (!lastInterpLogTime.has_value()) {
            shouldLog = true;
        } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *lastInterpLogTime).count() >=
                   tuning.interpLogPeriodMs) {
            shouldLog = true;
        }
        if (shouldLog) {
            lastInterpLogTime = now;
            std::fprintf(
                stderr,
                "[Garden interp] delay_smoothed=%u delay_want=%u rtt_s=%.3f extrap_remote=%u latest_clamp=%u "
                "interp_frames=%zu latest_tick=%u target_tick_f=%.2f "
                "clamp_min_speed=%.2f clamp_dv=%.2f max_extrap_ticks=%u vel_jump_blend_mps=%.2f "
                "vel_jump_extrap_mps=%.2f rtt_scale=%.3f smooth_max_ticks=%u\n",
                static_cast<unsigned>(interpDelaySmoothedTicks),
                static_cast<unsigned>(wantDelayTicks),
                static_cast<double>(rtt),
                static_cast<unsigned>(extrapRemoteCount),
                static_cast<unsigned>(latestClampCount),
                interp.frameCount(),
                static_cast<unsigned>(interp.latestTick()),
                static_cast<double>(targetTickF),
                static_cast<double>(tuning.clampMinSpeedMps),
                static_cast<double>(tuning.clampDvMps),
                static_cast<unsigned>(tuning.maxExtrapolationTicks),
                static_cast<double>(tuning.velocityJumpBlendMps),
                static_cast<double>(tuning.velocityJumpExtrapMps),
                static_cast<double>(tuning.interpRttScale),
                static_cast<unsigned>(tuning.interpDelaySmoothMaxTicks));
        }
    }
}

void gardenRemoteRunPredictReconcileAccum(
    GardenRemoteNetTuning const& tuning,
    float wallDeltaSeconds,
    float fixedPhysicsDt,
    float& fixedAccumInOut,
    std::size_t localSlot,
    GardenLayout const& layout,
    float pendingMoveX,
    float pendingMoveZ,
    std::uint8_t pendingButtons,
    bool& jumpWasHeld,
    float& jumpChargeSec,
    std::span<RigidBodyKinematics> marbles,
    std::span<Vec3 const> remoteAuthPos,
    std::span<bool const> remoteAuthValid,
    ClientSession<>& clientSession,
    SnapshotInterpolator<> const* interpDiag,
    float serverSimulationHz,
    std::uint32_t lastAckedServerSimTick,
    std::uint64_t& desyncHardSnapCount,
    std::optional<std::chrono::steady_clock::time_point>& lastDesyncLogTime
) noexcept {
    if (clientSession.state() != ConnectionState::Connected) {
        return;
    }
    PeerId const ap = clientSession.assignedPeerId();
    if (ap < 2u) {
        return;
    }
    std::size_t const slot = static_cast<std::size_t>(ap) - 2u;
    if (slot != localSlot || slot >= marbles.size()) {
        return;
    }

    if (tuning.noPredict) {
        if (tuning.desyncLog && slot < remoteAuthValid.size() && remoteAuthValid[slot] &&
            slot < remoteAuthPos.size()) {
            Vec3 const posAfterInterp = marbles[slot].position;
            auto const now = std::chrono::steady_clock::now();
            bool shouldLog = false;
            if (!lastDesyncLogTime.has_value()) {
                shouldLog = true;
            } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *lastDesyncLogTime).count() >=
                       tuning.desyncLogPeriodMs) {
                shouldLog = true;
            }
            if (shouldLog) {
                lastDesyncLogTime = now;
                Vec3 const authPos = remoteAuthPos[slot];
                float const interpErr = std::sqrt(marble::math::lengthSquared(posAfterInterp - authPos));
                std::fprintf(
                    stderr,
                    "[Garden desync] no_pred=1 rtt_s=%.3f ack_tick=%u "
                    "pos_err_interp_m=%.4f pos_err_after_predict_reconcile_m=%.4f "
                    "hard_snaps=%llu snap_thresh_m=%.3f\n",
                    static_cast<double>(clientSession.estimatedRttSeconds()),
                    static_cast<unsigned>(lastAckedServerSimTick),
                    static_cast<double>(interpErr),
                    static_cast<double>(interpErr),
                    static_cast<unsigned long long>(desyncHardSnapCount),
                    static_cast<double>(kGardenRemoteReconcileSnapThresholdM));
                if (interpDiag != nullptr && interpErr > 0.25f) {
                    float const targetTickF =
                        clientSession.estimatedServerSimTickAtNow(serverSimulationHz) -
                        static_cast<float>(interpDiag->renderDelayTicks());
                    std::fprintf(
                        stderr,
                        "  [Garden desync detail] target_tick_f=%.2f interp_latest_tick=%u interp_frames=%u "
                        "interp_delay_ticks=%u sim_hz=%.1f\n",
                        static_cast<double>(targetTickF),
                        static_cast<unsigned>(interpDiag->latestTick()),
                        static_cast<unsigned>(interpDiag->frameCount()),
                        static_cast<unsigned>(interpDiag->renderDelayTicks()),
                        static_cast<double>(serverSimulationHz));
                }
            }
        }
        return;
    }

    if (!(fixedPhysicsDt > 0.f)) {
        return;
    }

    Vec3 const posAfterInterp = marbles[slot].position;

    fixedAccumInOut += wallDeltaSeconds;
    int guard = 0;
    // Cap catch-up steps so a long frame cannot storm reconcile snaps against authority.
    static constexpr int kMaxPredictStepsPerFrame = 8;
    while (fixedAccumInOut >= fixedPhysicsDt && guard < kMaxPredictStepsPerFrame) {
        ++guard;
        fixedAccumInOut -= fixedPhysicsDt;

        applyGardenMarblePlayerStep(
            marbles[slot],
            layout,
            pendingMoveX,
            pendingMoveZ,
            (pendingButtons & kClientInputButton_Jump) != 0,
            jumpWasHeld,
            jumpChargeSec,
            fixedPhysicsDt,
            kMarbleRadius);

        if (slot < remoteAuthValid.size() && remoteAuthValid[slot] && slot < remoteAuthPos.size()) {
            RemoteAuthorityReconcileResult const rec = reconcileEmergencyPositionSnap(
                marbles[slot].position,
                remoteAuthPos[slot],
                kGardenRemoteReconcileSnapThresholdM);
            if (rec.hardSnapped) {
                ++desyncHardSnapCount;
            }
        }
    }

    Vec3 const posAfterReconcile = marbles[slot].position;

    if (tuning.desyncLog && slot < remoteAuthValid.size() && remoteAuthValid[slot] &&
        slot < remoteAuthPos.size()) {
        auto const now = std::chrono::steady_clock::now();
        bool shouldLog = false;
        if (!lastDesyncLogTime.has_value()) {
            shouldLog = true;
        } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *lastDesyncLogTime).count() >=
                   tuning.desyncLogPeriodMs) {
            shouldLog = true;
        }
        if (shouldLog) {
            lastDesyncLogTime = now;
            Vec3 const authPos = remoteAuthPos[slot];
            float const interpErr = std::sqrt(marble::math::lengthSquared(posAfterInterp - authPos));
            float const predErr = std::sqrt(marble::math::lengthSquared(posAfterReconcile - authPos));
            float const reconciledErr = std::sqrt(marble::math::lengthSquared(posAfterReconcile - authPos));
            (void)predErr;
            std::fprintf(
                stderr,
                "[Garden desync] no_pred=%d rtt_s=%.3f ack_tick=%u "
                "pos_err_interp_m=%.4f pos_err_after_predict_reconcile_m=%.4f "
                "hard_snaps=%llu snap_thresh_m=%.3f\n",
                0,
                static_cast<double>(clientSession.estimatedRttSeconds()),
                static_cast<unsigned>(lastAckedServerSimTick),
                static_cast<double>(interpErr),
                static_cast<double>(reconciledErr),
                static_cast<unsigned long long>(desyncHardSnapCount),
                static_cast<double>(kGardenRemoteReconcileSnapThresholdM));
            if (interpDiag != nullptr && interpErr > 0.25f) {
                float const targetTickF =
                    clientSession.estimatedServerSimTickAtNow(serverSimulationHz) -
                    static_cast<float>(interpDiag->renderDelayTicks());
                std::fprintf(
                    stderr,
                "  [Garden desync detail] target_tick_f=%.2f interp_latest_tick=%u interp_frames=%u "
                "interp_delay_ticks=%u sim_hz=%.1f\n",
                    static_cast<double>(targetTickF),
                    static_cast<unsigned>(interpDiag->latestTick()),
                    static_cast<unsigned>(interpDiag->frameCount()),
                    static_cast<unsigned>(interpDiag->renderDelayTicks()),
                    static_cast<double>(serverSimulationHz));
            }
        }
    }
}

} // namespace marble::garden_app
