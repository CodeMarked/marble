#pragma once

#include "gameplay/MultiplayerWireFormat.hpp"
#include "math/Vec3.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

/// Interpolated entity state produced by SnapshotInterpolator.
/// `extrapolated` is true when the result is NOT a proper lerp between two
/// bracketing server snapshots (single frame, render tick outside range, or
/// entity present in only one of the two bracketing frames).
struct InterpolatedEntity {
    WorldObjectRef entity{};
    math::Vec3 position{};
    math::Vec3 velocity{};
    float yawRadians{};
    PhysicsSimulationTier tier{PhysicsSimulationTier::Contact};
    bool extrapolated{};
};

/// Client-side snapshot interpolator: buffers decoded server snapshots and
/// produces smooth per-entity state at an arbitrary render tick in server
/// simTick space ([ADR-0060] §2 interpolation delay baseline).
///
/// Operates in **server simTick** coordinates. Callers pass a `float` render tick
/// (fractional allowed) from wall-clock mapping or `static_cast<float>(suggestRenderTick())`.
///
/// Extrapolation past the newest keyframe integrates `linearVelocity * dt` where
/// `dt` is **seconds** derived from tick delta ÷ [`simulationHz()`] (defaults to 60 Hz).
template <std::size_t MaxEntities = 16, std::size_t TimelineCapacity = 16>
class SnapshotInterpolator {
public:
    static_assert(TimelineCapacity >= 2, "Need at least 2 slots for interpolation");
    static_assert(MaxEntities > 0, "Need at least 1 entity slot");

    static constexpr std::uint32_t kDefaultRenderDelayTicks = 6u;
    static constexpr std::uint32_t kDefaultMaxExtrapolationTicks = 6u;

    struct Frame {
        std::uint32_t simTick{};
        std::array<EntityKinematicsSnapshot, MaxEntities> entities{};
        std::size_t entityCount{};
        bool valid{};
    };

    void reset() noexcept {
        frameHead_ = 0u;
        frameCount_ = 0u;
        for (auto& f : frames_) {
            f = {};
        }
    }

    void setRenderDelayTicks(std::uint32_t ticks) noexcept { renderDelayTicks_ = ticks; }
    void setMaxExtrapolationTicks(std::uint32_t ticks) noexcept { maxExtrapolationTicks_ = ticks; }

    /// Server fixed-step rate (Hz); used to convert extrapolation tick deltas to seconds. Default 60.
    void setSimulationHz(float hz) noexcept { simulationHz_ = hz > 1e-5f ? hz : 60.f; }
    [[nodiscard]] float simulationHz() const noexcept { return simulationHz_; }

    /// 0 = disabled. When \|v_new − v_old\| between bracketing frames meets threshold, lerp position but use **newer** velocity (avoid blending through impulses).
    void setVelocityJumpBlendThresholdMps(float metersPerSecond) noexcept {
        velocityJumpBlendThresholdMps_ = metersPerSecond > 0.f ? metersPerSecond : 0.f;
    }

    /// 0 = disabled. When extrapolating past the newest frame, per-entity integration uses dt = 0 if velocity jumped vs the previous frame by at least this much.
    void setVelocityJumpExtrapolationThresholdMps(float metersPerSecond) noexcept {
        velocityJumpExtrapolationThresholdMps_ = metersPerSecond > 0.f ? metersPerSecond : 0.f;
    }

    [[nodiscard]] std::uint32_t renderDelayTicks() const noexcept { return renderDelayTicks_; }
    [[nodiscard]] std::uint32_t maxExtrapolationTicks() const noexcept { return maxExtrapolationTicks_; }
    [[nodiscard]] float velocityJumpBlendThresholdMps() const noexcept { return velocityJumpBlendThresholdMps_; }
    [[nodiscard]] float velocityJumpExtrapolationThresholdMps() const noexcept {
        return velocityJumpExtrapolationThresholdMps_;
    }
    [[nodiscard]] std::size_t frameCount() const noexcept { return frameCount_; }

    void pushSnapshot(
        std::uint32_t simTick,
        EntityKinematicsSnapshot const* entities,
        std::size_t count
    ) noexcept {
        if (entities == nullptr && count > 0u) {
            return;
        }
        Frame& f = frames_[frameHead_];
        f.simTick = simTick;
        f.entityCount = (count < MaxEntities) ? count : MaxEntities;
        for (std::size_t i = 0u; i < f.entityCount; ++i) {
            f.entities[i] = entities[i];
        }
        f.valid = true;
        frameHead_ = (frameHead_ + 1u) % TimelineCapacity;
        if (frameCount_ < TimelineCapacity) {
            ++frameCount_;
        }
    }

    [[nodiscard]] std::uint32_t latestTick() const noexcept {
        if (frameCount_ == 0u) {
            return 0u;
        }
        return frameAtIndex(frameCount_ - 1u)->simTick;
    }

    /// Suggested render tick: latest server tick minus the configured delay.
    [[nodiscard]] std::uint32_t suggestRenderTick() const noexcept {
        std::uint32_t const latest = latestTick();
        return (latest > renderDelayTicks_) ? (latest - renderDelayTicks_) : 0u;
    }

    /// Newest buffered frame only: copy kinematics for `ref` if present (for clamping extrapolated remote poses).
    [[nodiscard]] bool tryLatestKinematics(WorldObjectRef ref, EntityKinematicsSnapshot& out) const noexcept {
        if (frameCount_ == 0u) {
            return false;
        }
        Frame const& f = *frameAtIndex(frameCount_ - 1u);
        for (std::size_t i = 0u; i < f.entityCount; ++i) {
            if (f.entities[i].entity == ref) {
                out = f.entities[i];
                return true;
            }
        }
        return false;
    }

    /// \|v_newest − v_penultimate\| for `ref` if present in both frames; false if fewer than two frames or entity missing from either.
    [[nodiscard]] bool tryVelocityDeltaBetweenLastTwoFrames(WorldObjectRef ref, float& outDeltaMps) const noexcept {
        if (frameCount_ < 2u) {
            return false;
        }
        Frame const& older = *frameAtIndex(frameCount_ - 2u);
        Frame const& newer = *frameAtIndex(frameCount_ - 1u);
        EntityKinematicsSnapshot const* ea = findEntity(older, ref);
        EntityKinematicsSnapshot const* eb = findEntity(newer, ref);
        if (ea == nullptr || eb == nullptr) {
            return false;
        }
        outDeltaMps = math::length(eb->linearVelocity - ea->linearVelocity);
        return std::isfinite(outDeltaMps);
    }

    /// Produce interpolated entity state at `renderTick` (server simTick space).
    /// Fractional values enable sub-tick blending between snapshot frames.
    [[nodiscard]] std::size_t interpolate(
        float renderTick,
        InterpolatedEntity* out,
        std::size_t maxOut
    ) const noexcept {
        if (out == nullptr || maxOut == 0u || frameCount_ == 0u) {
            return 0u;
        }

        if (frameCount_ == 1u) {
            return emitFrame(*frameAtIndex(0u), out, maxOut, true);
        }

        Frame const* older = nullptr;
        Frame const* newer = nullptr;

        for (std::size_t i = 0u; i + 1u < frameCount_; ++i) {
            Frame const* a = frameAtIndex(i);
            Frame const* b = frameAtIndex(i + 1u);
            float const aTick = static_cast<float>(a->simTick);
            float const bTick = static_cast<float>(b->simTick);
            if (aTick <= renderTick && bTick >= renderTick) {
                older = a;
                newer = b;
                break;
            }
        }

        if (older != nullptr && newer != nullptr) {
            return interpolateFrames(*older, *newer, renderTick, out, maxOut);
        }

        Frame const* oldest = frameAtIndex(0u);
        if (renderTick < static_cast<float>(oldest->simTick)) {
            return emitFrame(*oldest, out, maxOut, true);
        }

        return extrapolateFrame(*frameAtIndex(frameCount_ - 1u), renderTick, out, maxOut);
    }

private:
    [[nodiscard]] Frame const* frameAtIndex(std::size_t i) const noexcept {
        std::size_t const base =
            (frameHead_ + TimelineCapacity - frameCount_) % TimelineCapacity;
        return &frames_[(base + i) % TimelineCapacity];
    }

    [[nodiscard]] static std::size_t emitFrame(
        Frame const& frame,
        InterpolatedEntity* out,
        std::size_t maxOut,
        bool extrapolated
    ) noexcept {
        std::size_t const count = (frame.entityCount < maxOut) ? frame.entityCount : maxOut;
        for (std::size_t i = 0u; i < count; ++i) {
            auto const& e = frame.entities[i];
            out[i] = InterpolatedEntity{
                e.entity, e.positionLocal, e.linearVelocity, e.yawRadians, e.tier, extrapolated};
        }
        return count;
    }

    [[nodiscard]] std::size_t interpolateFrames(
        Frame const& older,
        Frame const& newer,
        float renderTick,
        InterpolatedEntity* out,
        std::size_t maxOut
    ) const noexcept {
        float const olderF = static_cast<float>(older.simTick);
        float const newerF = static_cast<float>(newer.simTick);
        float const span = newerF - olderF;
        float const t = (span > 0.f) ? (renderTick - olderF) / span : 0.f;

        std::size_t count = 0u;

        for (std::size_t i = 0u; i < older.entityCount && count < maxOut; ++i) {
            auto const& ea = older.entities[i];
            EntityKinematicsSnapshot const* eb = findEntity(newer, ea.entity);
            if (eb != nullptr) {
                math::Vec3 const pos = math::lerp(ea.positionLocal, eb->positionLocal, t);
                bool const jumpVel = velocityJumpBlendThresholdMps_ > 0.f &&
                    math::length(eb->linearVelocity - ea.linearVelocity) >= velocityJumpBlendThresholdMps_;
                math::Vec3 const vel =
                    jumpVel ? eb->linearVelocity
                            : math::lerp(ea.linearVelocity, eb->linearVelocity, t);
                out[count++] = InterpolatedEntity{
                    ea.entity,
                    pos,
                    vel,
                    math::lerpAngleRadians(ea.yawRadians, eb->yawRadians, t),
                    (t < 0.5f) ? ea.tier : eb->tier,
                    false
                };
            } else {
                out[count++] = InterpolatedEntity{
                    ea.entity,
                    ea.positionLocal,
                    ea.linearVelocity,
                    ea.yawRadians,
                    ea.tier,
                    true
                };
            }
        }

        for (std::size_t i = 0u; i < newer.entityCount && count < maxOut; ++i) {
            auto const& eb = newer.entities[i];
            if (findEntity(older, eb.entity) == nullptr) {
                out[count++] = InterpolatedEntity{
                    eb.entity,
                    eb.positionLocal,
                    eb.linearVelocity,
                    eb.yawRadians,
                    eb.tier,
                    true
                };
            }
        }

        return count;
    }

    [[nodiscard]] std::size_t extrapolateFrame(
        Frame const& frame,
        float renderTick,
        InterpolatedEntity* out,
        std::size_t maxOut
    ) const noexcept {
        float const deltaTicks = std::max(0.f, renderTick - static_cast<float>(frame.simTick));
        float const extrapTicks = std::min(deltaTicks, static_cast<float>(maxExtrapolationTicks_));
        float const dtDefault = extrapTicks / simulationHz_;

        Frame const* prev = nullptr;
        if (frameCount_ >= 2u && velocityJumpExtrapolationThresholdMps_ > 0.f) {
            prev = frameAtIndex(frameCount_ - 2u);
        }

        std::size_t const count = (frame.entityCount < maxOut) ? frame.entityCount : maxOut;
        for (std::size_t i = 0u; i < count; ++i) {
            auto const& e = frame.entities[i];
            float dt = dtDefault;
            if (prev != nullptr) {
                EntityKinematicsSnapshot const* ep = findEntity(*prev, e.entity);
                if (ep != nullptr) {
                    float const dv = math::length(e.linearVelocity - ep->linearVelocity);
                    if (dv >= velocityJumpExtrapolationThresholdMps_) {
                        dt = 0.f;
                    }
                }
            }
            out[i] = InterpolatedEntity{
                e.entity,
                e.positionLocal + e.linearVelocity * dt,
                e.linearVelocity,
                e.yawRadians,
                e.tier,
                true
            };
        }
        return count;
    }

    [[nodiscard]] static EntityKinematicsSnapshot const* findEntity(
        Frame const& frame,
        WorldObjectRef ref
    ) noexcept {
        for (std::size_t i = 0u; i < frame.entityCount; ++i) {
            if (frame.entities[i].entity == ref) {
                return &frame.entities[i];
            }
        }
        return nullptr;
    }

    std::array<Frame, TimelineCapacity> frames_{};
    std::size_t frameHead_{};
    std::size_t frameCount_{};
    std::uint32_t renderDelayTicks_{kDefaultRenderDelayTicks};
    std::uint32_t maxExtrapolationTicks_{kDefaultMaxExtrapolationTicks};
    float simulationHz_{60.f};
    float velocityJumpBlendThresholdMps_{};
    float velocityJumpExtrapolationThresholdMps_{};
};

} // namespace marble::gameplay
