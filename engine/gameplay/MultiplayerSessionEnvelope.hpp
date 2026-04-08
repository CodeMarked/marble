#pragma once

#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <cstring>

namespace marble::gameplay {

/// Little-endian wire magic `MRB1` — reject random internet noise before parsing ([ADR-0060]).
inline constexpr std::uint32_t kMarbleSessionMagicLe = 0x3142524du;

/// Bump when envelope or message layouts change; peers must match exactly.
inline constexpr std::uint16_t kMarbleSessionProtocolVersion = 2u;

inline constexpr std::size_t kSessionEnvelopeBytes = 8u;

/// Flags byte (envelope byte 7): bit 0 set means an 8-byte reliable header follows the envelope.
inline constexpr std::uint8_t kSessionEnvelopeFlag_Reliable = 0x01u;

/// Reliable header size: [sequence:u16][ackSequence:u16][ackBitmap:u32].
inline constexpr std::size_t kReliableHeaderBytes = 8u;

enum class SessionMessageType : std::uint8_t {
    Hello = 1,
    HelloAck = 2,
    GameSnapshot = 3,
    Ack = 4,
    Disconnect = 5,
    ClientInput = 6
};

struct SessionHelloPayload {
    std::uint16_t clientUdpPortHost{};
    std::uint32_t clientNonce{};
};

inline constexpr std::size_t kSessionHelloPayloadBytes = 6u;

struct SessionHelloAckPayload {
    PeerId assignedPeerId{kInvalidPeerId};
    std::uint16_t reserved{};
    std::uint32_t echoClientNonce{};
};

inline constexpr std::size_t kSessionHelloAckPayloadBytes = 8u;

// ---------------------------------------------------------------------------
// Unreliable envelope (flags byte = 0, no reliable header)
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::size_t writeSessionEnvelope(
    std::uint8_t* out,
    std::size_t cap,
    SessionMessageType msgType,
    void const* payload,
    std::size_t payloadBytes,
    std::uint8_t flags = 0u
) noexcept {
    std::size_t const total = kSessionEnvelopeBytes + payloadBytes;
    if (out == nullptr || cap < total) {
        return 0u;
    }
    writeU32Le(out + 0u, kMarbleSessionMagicLe);
    writeU16Le(out + 4u, kMarbleSessionProtocolVersion);
    writeU8(out + 6u, static_cast<std::uint8_t>(msgType));
    writeU8(out + 7u, flags);
    if (payloadBytes > 0u && payload != nullptr) {
        std::memcpy(out + kSessionEnvelopeBytes, payload, payloadBytes);
    }
    return total;
}

// ---------------------------------------------------------------------------
// Reliable envelope (flags byte has bit 0 set, 8-byte reliable header after envelope)
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::size_t writeSessionEnvelopeReliable(
    std::uint8_t* out,
    std::size_t cap,
    SessionMessageType msgType,
    std::uint16_t seq,
    std::uint16_t ackSeq,
    std::uint32_t ackBits,
    void const* payload,
    std::size_t payloadBytes
) noexcept {
    std::size_t const total = kSessionEnvelopeBytes + kReliableHeaderBytes + payloadBytes;
    if (out == nullptr || cap < total) {
        return 0u;
    }
    writeU32Le(out + 0u, kMarbleSessionMagicLe);
    writeU16Le(out + 4u, kMarbleSessionProtocolVersion);
    writeU8(out + 6u, static_cast<std::uint8_t>(msgType));
    writeU8(out + 7u, kSessionEnvelopeFlag_Reliable);
    writeU16Le(out + 8u, seq);
    writeU16Le(out + 10u, ackSeq);
    writeU32Le(out + 12u, ackBits);
    if (payloadBytes > 0u && payload != nullptr) {
        std::memcpy(out + kSessionEnvelopeBytes + kReliableHeaderBytes, payload, payloadBytes);
    }
    return total;
}

// ---------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------

/// Extended parse that also outputs the flags byte.
/// `outPayload` points past the envelope header (byte 8); for reliable messages
/// the first `kReliableHeaderBytes` of `outPayload` are the reliable header.
[[nodiscard]] inline bool parseSessionEnvelopeEx(
    std::uint8_t const* in,
    std::size_t len,
    SessionMessageType& outType,
    std::uint8_t& outFlags,
    std::uint8_t const*& outPayload,
    std::size_t& outPayloadLen
) noexcept {
    if (in == nullptr || len < kSessionEnvelopeBytes) {
        return false;
    }
    std::uint32_t const magic = readU32Le(in + 0u);
    std::uint16_t const ver = readU16Le(in + 4u);
    if (magic != kMarbleSessionMagicLe || ver != kMarbleSessionProtocolVersion) {
        return false;
    }
    outType = static_cast<SessionMessageType>(readU8(in + 6u));
    outFlags = readU8(in + 7u);
    outPayload = in + kSessionEnvelopeBytes;
    outPayloadLen = len - kSessionEnvelopeBytes;
    return true;
}

/// Original parse (ignores flags). Calls [`parseSessionEnvelopeEx`] internally.
[[nodiscard]] inline bool parseSessionEnvelope(
    std::uint8_t const* in,
    std::size_t len,
    SessionMessageType& outType,
    std::uint8_t const*& outPayload,
    std::size_t& outPayloadLen
) noexcept {
    std::uint8_t flags{};
    return parseSessionEnvelopeEx(in, len, outType, flags, outPayload, outPayloadLen);
}

/// Read the 8-byte reliable header from the start of the payload area.
[[nodiscard]] inline bool readReliableHeader(
    std::uint8_t const* data,
    std::size_t len,
    std::uint16_t& outSeq,
    std::uint16_t& outAckSeq,
    std::uint32_t& outAckBits
) noexcept {
    if (data == nullptr || len < kReliableHeaderBytes) {
        return false;
    }
    outSeq = readU16Le(data + 0u);
    outAckSeq = readU16Le(data + 2u);
    outAckBits = readU32Le(data + 4u);
    return true;
}

// ---------------------------------------------------------------------------
// Message-specific write / read helpers
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::size_t writeSessionHello(
    std::uint8_t* out,
    std::size_t cap,
    SessionHelloPayload const& p
) noexcept {
    std::array<std::uint8_t, kSessionHelloPayloadBytes> pl{};
    writeU16Le(pl.data(), p.clientUdpPortHost);
    writeU32Le(pl.data() + 2u, p.clientNonce);
    return writeSessionEnvelope(out, cap, SessionMessageType::Hello, pl.data(), pl.size());
}

[[nodiscard]] inline bool readSessionHello(
    std::uint8_t const* payload,
    std::size_t payloadLen,
    SessionHelloPayload& out
) noexcept {
    if (payload == nullptr || payloadLen < kSessionHelloPayloadBytes) {
        return false;
    }
    out.clientUdpPortHost = readU16Le(payload + 0u);
    out.clientNonce = readU32Le(payload + 2u);
    return true;
}

/// Unreliable HelloAck (legacy; prefer [`writeSessionHelloAckReliable`] for new code).
[[nodiscard]] inline std::size_t writeSessionHelloAck(
    std::uint8_t* out,
    std::size_t cap,
    SessionHelloAckPayload const& p
) noexcept {
    std::array<std::uint8_t, kSessionHelloAckPayloadBytes> pl{};
    writeU16Le(pl.data(), p.assignedPeerId);
    writeU16Le(pl.data() + 2u, p.reserved);
    writeU32Le(pl.data() + 4u, p.echoClientNonce);
    return writeSessionEnvelope(out, cap, SessionMessageType::HelloAck, pl.data(), pl.size());
}

/// Reliable HelloAck with sequence + ack header.
[[nodiscard]] inline std::size_t writeSessionHelloAckReliable(
    std::uint8_t* out,
    std::size_t cap,
    SessionHelloAckPayload const& p,
    std::uint16_t seq,
    std::uint16_t ackSeq,
    std::uint32_t ackBits
) noexcept {
    std::array<std::uint8_t, kSessionHelloAckPayloadBytes> pl{};
    writeU16Le(pl.data(), p.assignedPeerId);
    writeU16Le(pl.data() + 2u, p.reserved);
    writeU32Le(pl.data() + 4u, p.echoClientNonce);
    return writeSessionEnvelopeReliable(out, cap, SessionMessageType::HelloAck,
                                        seq, ackSeq, ackBits, pl.data(), pl.size());
}

[[nodiscard]] inline bool readSessionHelloAck(
    std::uint8_t const* payload,
    std::size_t payloadLen,
    SessionHelloAckPayload& out
) noexcept {
    if (payload == nullptr || payloadLen < kSessionHelloAckPayloadBytes) {
        return false;
    }
    out.assignedPeerId = readU16Le(payload + 0u);
    out.reserved = readU16Le(payload + 2u);
    out.echoClientNonce = readU32Le(payload + 4u);
    return true;
}

/// Pure ack (reliable, no payload beyond the sequence/ack header).
[[nodiscard]] inline std::size_t writeSessionAck(
    std::uint8_t* out,
    std::size_t cap,
    std::uint16_t seq,
    std::uint16_t ackSeq,
    std::uint32_t ackBits
) noexcept {
    return writeSessionEnvelopeReliable(out, cap, SessionMessageType::Ack,
                                        seq, ackSeq, ackBits, nullptr, 0u);
}

/// Reliable disconnect notification (no payload beyond the sequence/ack header).
[[nodiscard]] inline std::size_t writeSessionDisconnect(
    std::uint8_t* out,
    std::size_t cap,
    std::uint16_t seq,
    std::uint16_t ackSeq,
    std::uint32_t ackBits
) noexcept {
    return writeSessionEnvelopeReliable(out, cap, SessionMessageType::Disconnect,
                                        seq, ackSeq, ackBits, nullptr, 0u);
}

/// Wrap an existing game payload (e.g. [`EntityKinematicsSnapshot`](MultiplayerWireFormat.hpp) wire) as [`SessionMessageType::GameSnapshot`].
[[nodiscard]] inline std::size_t writeSessionGameSnapshot(
    std::uint8_t* out,
    std::size_t cap,
    void const* gamePayload,
    std::size_t gamePayloadBytes
) noexcept {
    return writeSessionEnvelope(out, cap, SessionMessageType::GameSnapshot, gamePayload, gamePayloadBytes);
}

[[nodiscard]] inline bool parseSessionGameSnapshot(
    std::uint8_t const* in,
    std::size_t len,
    std::uint8_t const*& outGamePayload,
    std::size_t& outGameLen
) noexcept {
    SessionMessageType t{};
    std::uint8_t const* pl{};
    std::size_t plen{};
    if (!parseSessionEnvelope(in, len, t, pl, plen) || t != SessionMessageType::GameSnapshot) {
        return false;
    }
    outGamePayload = pl;
    outGameLen = plen;
    return true;
}

/// Wrap a [`ClientInputWirePayload`](MultiplayerWireFormat.hpp) as [`SessionMessageType::ClientInput`].
[[nodiscard]] inline std::size_t writeSessionClientInput(
    std::uint8_t* out,
    std::size_t cap,
    void const* inputPayload,
    std::size_t inputPayloadBytes
) noexcept {
    return writeSessionEnvelope(out, cap, SessionMessageType::ClientInput, inputPayload, inputPayloadBytes);
}

} // namespace marble::gameplay
