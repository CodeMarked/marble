#pragma once

#include "gameplay/MultiplayerWireFormat.hpp"

#include <array>
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
    PhysicsSimulationTier tier{PhysicsSimulationTier::Contact};
    bool extrapolated{};
};

/// Client-side snapshot interpolator: buffers decoded server snapshots and
/// produces smooth per-entity state at an arbitrary render tick in server
/// simTick space ([ADR-0060] §2 interpolation delay baseline).
///
/// Operates entirely in **server simTick** coordinates. The caller converts
/// client time to a server-space render tick (or uses `suggestRenderTick()`).
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

    [[nodiscard]] std::uint32_t renderDelayTicks() const noexcept { return renderDelayTicks_; }
    [[nodiscard]] std::uint32_t maxExtrapolationTicks() const noexcept { return maxExtrapolationTicks_; }
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

    /// Produce interpolated entity state at `renderTick` (server simTick space).
    [[nodiscard]] std::size_t interpolate(
        std::uint32_t renderTick,
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
            if (a->simTick <= renderTick && b->simTick >= renderTick) {
                older = a;
                newer = b;
                break;
            }
        }

        if (older != nullptr && newer != nullptr) {
            return interpolateFrames(*older, *newer, renderTick, out, maxOut);
        }

        Frame const* oldest = frameAtIndex(0u);
        if (renderTick < oldest->simTick) {
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
            out[i] = InterpolatedEntity{e.entity, e.positionLocal, e.linearVelocity, e.tier, extrapolated};
        }
        return count;
    }

    [[nodiscard]] static std::size_t interpolateFrames(
        Frame const& older,
        Frame const& newer,
        std::uint32_t renderTick,
        InterpolatedEntity* out,
        std::size_t maxOut
    ) noexcept {
        std::uint32_t const span = newer.simTick - older.simTick;
        float const t = (span > 0u)
            ? static_cast<float>(renderTick - older.simTick) / static_cast<float>(span)
            : 0.f;

        std::size_t count = 0u;

        for (std::size_t i = 0u; i < older.entityCount && count < maxOut; ++i) {
            auto const& ea = older.entities[i];
            EntityKinematicsSnapshot const* eb = findEntity(newer, ea.entity);
            if (eb != nullptr) {
                out[count++] = InterpolatedEntity{
                    ea.entity,
                    math::lerp(ea.positionLocal, eb->positionLocal, t),
                    math::lerp(ea.linearVelocity, eb->linearVelocity, t),
                    (t < 0.5f) ? ea.tier : eb->tier,
                    false
                };
            } else {
                out[count++] = InterpolatedEntity{
                    ea.entity, ea.positionLocal, ea.linearVelocity, ea.tier, true
                };
            }
        }

        for (std::size_t i = 0u; i < newer.entityCount && count < maxOut; ++i) {
            auto const& eb = newer.entities[i];
            if (findEntity(older, eb.entity) == nullptr) {
                out[count++] = InterpolatedEntity{
                    eb.entity, eb.positionLocal, eb.linearVelocity, eb.tier, true
                };
            }
        }

        return count;
    }

    [[nodiscard]] std::size_t extrapolateFrame(
        Frame const& frame,
        std::uint32_t renderTick,
        InterpolatedEntity* out,
        std::size_t maxOut
    ) const noexcept {
        std::uint32_t const delta = renderTick - frame.simTick;
        std::uint32_t const clamped =
            (delta <= maxExtrapolationTicks_) ? delta : maxExtrapolationTicks_;
        float const dt = static_cast<float>(clamped);

        std::size_t const count = (frame.entityCount < maxOut) ? frame.entityCount : maxOut;
        for (std::size_t i = 0u; i < count; ++i) {
            auto const& e = frame.entities[i];
            out[i] = InterpolatedEntity{
                e.entity,
                e.positionLocal + e.linearVelocity * dt,
                e.linearVelocity,
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
};

} // namespace marble::gameplay
