#pragma once

#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/WorldDataFormats.hpp"
#include "math/Vec3.hpp"

#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

/// Small replicated entity state driven by external simulation and emitted as snapshots.
struct ReplicatedEntity {
    WorldObjectRef entity{};
    math::Vec3 position{};
    math::Vec3 velocity{};
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

/// Per-peer AOI state stored alongside session peer data.
struct PeerInterest {
    InterestRegion region{};
    std::size_t viewEntityIndex{};
    bool aoiEnabled{};
};

} // namespace marble::gameplay
