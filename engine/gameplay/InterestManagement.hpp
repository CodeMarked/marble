#pragma once

#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/WorldDataFormats.hpp"
#include "math/Vec3.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

/// Small replicated entity state driven by external simulation and emitted as snapshots.
struct ReplicatedEntity {
    WorldObjectRef entity{};
    math::Vec3 position{};
    math::Vec3 velocity{};
    float yawRadians{};
    PhysicsSimulationTier tier{PhysicsSimulationTier::Contact};
    bool active{};
};

/// Per-peer interest region for area-of-interest filtering.
/// Entities outside `radius` from `viewPosition` are excluded from that peer's snapshot.
/// For fast movers, the effective radius is expanded by `speed * velocityLookaheadSeconds`.
struct InterestRegion {
    math::Vec3 viewPosition{};
    float radius = 500.f;
    float velocityLookaheadSeconds = 0.5f;
};

/// Filter a set of replicated entities for a single peer.
/// Returns the count of entities written to `out` (at most `maxOut`).
template <std::size_t MaxEntities>
[[nodiscard]] std::size_t filterEntitiesForPeer(
    ReplicatedEntity const* entities,
    std::size_t entityCount,
    InterestRegion const& region,
    ReplicatedEntity* out,
    std::size_t maxOut
) noexcept {
    std::size_t written = 0u;
    for (std::size_t i = 0u; i < entityCount && written < maxOut; ++i) {
        if (!entities[i].active) {
            continue;
        }
        math::Vec3 const delta = entities[i].position - region.viewPosition;
        float const dist2 = math::lengthSquared(delta);

        float effectiveR = region.radius;
        float const speed = math::length(entities[i].velocity);
        if (speed > 0.01f) {
            effectiveR += speed * region.velocityLookaheadSeconds;
        }
        float const effectiveR2 = effectiveR * effectiveR;

        if (dist2 <= effectiveR2) {
            out[written++] = entities[i];
        }
    }
    return written;
}

/// View pose for relevance scoring (velocity biases entities ahead of travel).
struct InterestViewContext {
    math::Vec3 position{};
    math::Vec3 velocity{};
};

/// Lower scores are replicated first (distance + frontal bias + mild speed preference).
[[nodiscard]] inline float interestScoreLowerIsBetter(
    ReplicatedEntity const& e,
    InterestViewContext const& view
) noexcept {
    math::Vec3 const d = e.position - view.position;
    float const dist2 = math::lengthSquared(d);
    float score = dist2;
    float const vs2 = math::lengthSquared(view.velocity);
    if (vs2 > 0.04f) {
        float const invV = 1.f / std::sqrt(vs2);
        math::Vec3 const vn = view.velocity * invV;
        float const dist = std::sqrt(std::max(dist2, 1e-8f));
        math::Vec3 const en = d * (1.f / dist);
        float const frontal = math::dot(vn, en);
        float const penal = 1.f + std::max(0.f, -frontal) * 2.2f;
        score *= penal;
    }
    float const es = math::length(e.velocity);
    score /= 1.f + es * 0.0025f;
    return score;
}

[[nodiscard]] inline std::uint32_t quantizedKinematicsFingerprint(
    ReplicatedEntity const& e
) noexcept {
    auto q = [](float x) noexcept -> std::uint32_t {
        return static_cast<std::uint32_t>(std::lround(x * 48.f)) * 0x9e3779b1u;
    };
    std::uint32_t h = 2166136261u;
    h ^= q(e.position.x) ^ q(e.position.y) ^ q(e.position.z);
    h ^= q(e.velocity.x) ^ q(e.velocity.y) ^ q(e.velocity.z);
    h ^= q(e.yawRadians);
    return h;
}

/// Per-peer AOI state stored alongside session peer data.
struct PeerInterest {
    InterestRegion region{};
    std::size_t viewEntityIndex{};
    bool aoiEnabled{};
};

} // namespace marble::gameplay
