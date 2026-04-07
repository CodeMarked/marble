#pragma once

#include "gameplay/WorldDataFormats.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "math/Vec3.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace marble::gameplay {

/// High-level physics integration tier for replication and handoffs ([ADR-0059]).
enum class PhysicsSimulationTier : std::uint8_t {
    Contact = 0,
    CruiseOrbit = 1
};

// --- Little-endian wire helpers (explicit; safe across host endianness) ---

[[nodiscard]] inline std::uint8_t readU8(std::uint8_t const* p) noexcept {
    return p[0];
}

[[nodiscard]] inline std::uint16_t readU16Le(std::uint8_t const* p) noexcept {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8u);
}

[[nodiscard]] inline std::uint32_t readU32Le(std::uint8_t const* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8u)
        | (static_cast<std::uint32_t>(p[2]) << 16u) | (static_cast<std::uint32_t>(p[3]) << 24u);
}

[[nodiscard]] inline std::uint64_t readU64Le(std::uint8_t const* p) noexcept {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(p[i]) << (8u * static_cast<unsigned>(i));
    }
    return v;
}

[[nodiscard]] inline double readF64Le(std::uint8_t const* p) noexcept {
    return std::bit_cast<double>(readU64Le(p));
}

[[nodiscard]] inline float readF32Le(std::uint8_t const* p) noexcept {
    return std::bit_cast<float>(readU32Le(p));
}

inline void writeU8(std::uint8_t* p, std::uint8_t v) noexcept {
    p[0] = v;
}

inline void writeU16Le(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v & 0xffu);
    p[1] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
}

inline void writeU32Le(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v & 0xffu);
    p[1] = static_cast<std::uint8_t>((v >> 8u) & 0xffu);
    p[2] = static_cast<std::uint8_t>((v >> 16u) & 0xffu);
    p[3] = static_cast<std::uint8_t>((v >> 24u) & 0xffu);
}

inline void writeU64Le(std::uint8_t* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<std::uint8_t>((v >> (8u * static_cast<unsigned>(i))) & 0xffu);
    }
}

inline void writeF64Le(std::uint8_t* p, double v) noexcept {
    writeU64Le(p, std::bit_cast<std::uint64_t>(v));
}

inline void writeF32Le(std::uint8_t* p, float v) noexcept {
    writeU32Le(p, std::bit_cast<std::uint32_t>(v));
}

/// Fixed layout: double originX, originY, originZ (little-endian IEEE-754).
inline constexpr std::size_t kSimulationIslandAnchorWireBytes = 24u;

[[nodiscard]] inline std::size_t writeSimulationIslandAnchor(
    std::uint8_t* out,
    std::size_t cap,
    SimulationIsland const& island
) noexcept {
    if (cap < kSimulationIslandAnchorWireBytes) {
        return 0u;
    }
    writeF64Le(out + 0u, island.originX);
    writeF64Le(out + 8u, island.originY);
    writeF64Le(out + 16u, island.originZ);
    return kSimulationIslandAnchorWireBytes;
}

[[nodiscard]] inline bool readSimulationIslandAnchor(
    std::uint8_t const* in,
    std::size_t len,
    SimulationIsland& outIsland
) noexcept {
    if (len < kSimulationIslandAnchorWireBytes) {
        return false;
    }
    outIsland.originX = readF64Le(in + 0u);
    outIsland.originY = readF64Le(in + 8u);
    outIsland.originZ = readF64Le(in + 16u);
    return true;
}

/// Authoritative tier handoff (reliable control channel): world pose + velocity + tier ([ADR-0059] H1–H3).
struct TierHandoffAuthoritative {
    std::uint32_t simTick{};
    WorldObjectRef entity{};
    PhysicsSimulationTier tier{PhysicsSimulationTier::Contact};
    WorldPosition3 worldPosition{};
    math::Vec3 linearVelocity{};
};

inline constexpr std::size_t kTierHandoffWireBytes = 56u;

[[nodiscard]] inline std::size_t writeTierHandoffAuthoritative(
    std::uint8_t* out,
    std::size_t cap,
    TierHandoffAuthoritative const& h
) noexcept {
    if (cap < kTierHandoffWireBytes) {
        return 0u;
    }
    writeU32Le(out + 0u, h.simTick);
    writeU64Le(out + 4u, h.entity.guid);
    writeU8(out + 12u, static_cast<std::uint8_t>(h.tier));
    std::memset(out + 13u, 0, 7u);
    writeF64Le(out + 20u, h.worldPosition.x);
    writeF64Le(out + 28u, h.worldPosition.y);
    writeF64Le(out + 36u, h.worldPosition.z);
    writeF32Le(out + 44u, h.linearVelocity.x);
    writeF32Le(out + 48u, h.linearVelocity.y);
    writeF32Le(out + 52u, h.linearVelocity.z);
    return kTierHandoffWireBytes;
}

[[nodiscard]] inline bool readTierHandoffAuthoritative(
    std::uint8_t const* in,
    std::size_t len,
    TierHandoffAuthoritative& out
) noexcept {
    if (len < kTierHandoffWireBytes) {
        return false;
    }
    out.simTick = readU32Le(in + 0u);
    out.entity.guid = readU64Le(in + 4u);
    out.tier = static_cast<PhysicsSimulationTier>(readU8(in + 12u));
    out.worldPosition.x = readF64Le(in + 20u);
    out.worldPosition.y = readF64Le(in + 28u);
    out.worldPosition.z = readF64Le(in + 36u);
    out.linearVelocity.x = readF32Le(in + 44u);
    out.linearVelocity.y = readF32Le(in + 48u);
    out.linearVelocity.z = readF32Le(in + 52u);
    return true;
}

/// Small unreliable state snapshot: simulation-local pose + velocity ([ADR-0060] narrow state).
struct EntityKinematicsSnapshot {
    std::uint32_t simTick{};
    WorldObjectRef entity{};
    PhysicsSimulationTier tier{PhysicsSimulationTier::Contact};
    math::Vec3 positionLocal{};
    math::Vec3 linearVelocity{};
};

inline constexpr std::size_t kEntityKinematicsSnapshotWireBytes = 44u;

[[nodiscard]] inline std::size_t writeEntityKinematicsSnapshot(
    std::uint8_t* out,
    std::size_t cap,
    EntityKinematicsSnapshot const& s
) noexcept {
    if (cap < kEntityKinematicsSnapshotWireBytes) {
        return 0u;
    }
    writeU32Le(out + 0u, s.simTick);
    writeU64Le(out + 4u, s.entity.guid);
    writeU8(out + 12u, static_cast<std::uint8_t>(s.tier));
    std::memset(out + 13u, 0, 3u);
    writeF32Le(out + 16u, s.positionLocal.x);
    writeF32Le(out + 20u, s.positionLocal.y);
    writeF32Le(out + 24u, s.positionLocal.z);
    writeF32Le(out + 28u, s.linearVelocity.x);
    writeF32Le(out + 32u, s.linearVelocity.y);
    writeF32Le(out + 36u, s.linearVelocity.z);
    return kEntityKinematicsSnapshotWireBytes;
}

[[nodiscard]] inline bool readEntityKinematicsSnapshot(
    std::uint8_t const* in,
    std::size_t len,
    EntityKinematicsSnapshot& out
) noexcept {
    if (len < kEntityKinematicsSnapshotWireBytes) {
        return false;
    }
    out.simTick = readU32Le(in + 0u);
    out.entity.guid = readU64Le(in + 4u);
    out.tier = static_cast<PhysicsSimulationTier>(readU8(in + 12u));
    out.positionLocal.x = readF32Le(in + 16u);
    out.positionLocal.y = readF32Le(in + 20u);
    out.positionLocal.z = readF32Le(in + 24u);
    out.linearVelocity.x = readF32Le(in + 28u);
    out.linearVelocity.y = readF32Le(in + 32u);
    out.linearVelocity.z = readF32Le(in + 36u);
    return true;
}

// ---------------------------------------------------------------------------
// Client input payload: world-space wish direction + buttons + client tick
// for future prediction support.  Wire: [clientTick:u32][moveX:f32][moveZ:f32][buttons:u8][pad:3] = 16 bytes
// ---------------------------------------------------------------------------

struct ClientInputWirePayload {
    std::uint32_t clientTick{};
    float moveX{};
    float moveZ{};
    std::uint8_t buttons{};
};

inline constexpr std::uint8_t kClientInputButton_Jump = 0x01u;
inline constexpr std::size_t kClientInputWirePayloadBytes = 16u;

[[nodiscard]] inline std::size_t writeClientInputPayload(
    std::uint8_t* out,
    std::size_t cap,
    ClientInputWirePayload const& p
) noexcept {
    if (out == nullptr || cap < kClientInputWirePayloadBytes) {
        return 0u;
    }
    writeU32Le(out + 0u, p.clientTick);
    writeF32Le(out + 4u, p.moveX);
    writeF32Le(out + 8u, p.moveZ);
    writeU8(out + 12u, p.buttons);
    std::memset(out + 13u, 0, 3u);
    return kClientInputWirePayloadBytes;
}

[[nodiscard]] inline bool readClientInputPayload(
    std::uint8_t const* in,
    std::size_t len,
    ClientInputWirePayload& out
) noexcept {
    if (in == nullptr || len < kClientInputWirePayloadBytes) {
        return false;
    }
    out.clientTick = readU32Le(in + 0u);
    out.moveX = readF32Le(in + 4u);
    out.moveZ = readF32Le(in + 8u);
    out.buttons = readU8(in + 12u);
    return true;
}

} // namespace marble::gameplay
