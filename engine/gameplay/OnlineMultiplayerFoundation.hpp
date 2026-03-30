#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

using PeerId = std::uint16_t;
inline constexpr PeerId kInvalidPeerId = 0u;

enum class MultiplayerMode : std::uint8_t {
    Offline,
    ListenServer,
    DedicatedServer
};

enum class ConnectionState : std::uint8_t {
    Disconnected,
    Connecting,
    Handshaking,
    Connected,
    TimingOut
};

[[nodiscard]] constexpr bool connectionTransitionAllowed(ConnectionState from, ConnectionState to) noexcept {
    if (from == to) {
        return false;
    }
    switch (from) {
    case ConnectionState::Disconnected:
        return to == ConnectionState::Connecting;
    case ConnectionState::Connecting:
        return to == ConnectionState::Handshaking || to == ConnectionState::Disconnected;
    case ConnectionState::Handshaking:
        return to == ConnectionState::Connected || to == ConnectionState::Disconnected;
    case ConnectionState::Connected:
        return to == ConnectionState::TimingOut || to == ConnectionState::Disconnected;
    case ConnectionState::TimingOut:
        return to == ConnectionState::Connected || to == ConnectionState::Disconnected;
    }
    return false;
}

struct SessionConfig {
    MultiplayerMode mode{MultiplayerMode::Offline};
    std::uint16_t maxPlayers{1u};
    std::uint16_t simulationHz{60u};
    std::uint16_t snapshotHz{20u};
    std::uint16_t maxPredictionTicks{2u};
};

[[nodiscard]] constexpr bool isValid(SessionConfig const& c) noexcept {
    if (c.maxPlayers == 0u || c.simulationHz == 0u || c.snapshotHz == 0u) {
        return false;
    }
    if (c.snapshotHz > c.simulationHz) {
        return false;
    }
    if (c.mode == MultiplayerMode::Offline) {
        return c.maxPlayers == 1u;
    }
    return c.maxPlayers >= 2u;
}

struct PlayerSlot {
    PeerId peer{kInvalidPeerId};
    ConnectionState state{ConnectionState::Disconnected};
    bool occupied{};
    bool authenticated{};
    bool localAuthority{}; // true for host/authority-side player slot
    std::uint32_t lastInputTick{};
};

template <std::size_t MaxPlayers>
class AuthorityRoster {
public:
    static_assert(MaxPlayers > 0, "AuthorityRoster requires positive capacity");

    [[nodiscard]] bool bootstrap(MultiplayerMode mode) noexcept {
        clear();
        mode_ = mode;
        if (mode_ == MultiplayerMode::DedicatedServer) {
            return true;
        }
        return addAuthorityPeer(1u);
    }

    [[nodiscard]] bool addAuthorityPeer(PeerId peer) noexcept {
        if (peer == kInvalidPeerId || mode_ == MultiplayerMode::DedicatedServer) {
            return false;
        }
        PlayerSlot* slot = findFreeSlot();
        if (slot == nullptr) {
            return false;
        }
        *slot = PlayerSlot{peer, ConnectionState::Connected, true, true, true, 0u};
        ++connectedCount_;
        return true;
    }

    [[nodiscard]] bool connectPeer(PeerId peer) noexcept {
        if (peer == kInvalidPeerId || findByPeer(peer) != nullptr) {
            return false;
        }
        PlayerSlot* slot = findFreeSlot();
        if (slot == nullptr) {
            return false;
        }
        *slot = PlayerSlot{peer, ConnectionState::Connecting, true, false, false, 0u};
        return true;
    }

    [[nodiscard]] bool transition(PeerId peer, ConnectionState to) noexcept {
        PlayerSlot* slot = findByPeer(peer);
        if (slot == nullptr) {
            return false;
        }
        if (!connectionTransitionAllowed(slot->state, to)) {
            return false;
        }
        slot->state = to;
        if (to == ConnectionState::Connected) {
            slot->authenticated = true;
            ++connectedCount_;
        } else if (to == ConnectionState::Disconnected) {
            if (slot->authenticated) {
                --connectedCount_;
            }
            *slot = {};
        }
        return true;
    }

    [[nodiscard]] bool setLastInputTick(PeerId peer, std::uint32_t tick) noexcept {
        PlayerSlot* slot = findByPeer(peer);
        if (slot == nullptr || !slot->authenticated) {
            return false;
        }
        if (tick < slot->lastInputTick) {
            return false;
        }
        slot->lastInputTick = tick;
        return true;
    }

    [[nodiscard]] bool isAuthoritativePeer(PeerId peer) const noexcept {
        PlayerSlot const* slot = findByPeer(peer);
        return slot != nullptr && slot->localAuthority;
    }

    [[nodiscard]] std::size_t connectedCount() const noexcept { return connectedCount_; }
    [[nodiscard]] MultiplayerMode mode() const noexcept { return mode_; }

    void clear() noexcept {
        for (PlayerSlot& slot : slots_) {
            slot = {};
        }
        connectedCount_ = 0u;
        mode_ = MultiplayerMode::Offline;
    }

private:
    [[nodiscard]] PlayerSlot* findFreeSlot() noexcept {
        for (PlayerSlot& slot : slots_) {
            if (!slot.occupied) {
                return &slot;
            }
        }
        return nullptr;
    }

    [[nodiscard]] PlayerSlot* findByPeer(PeerId peer) noexcept {
        for (PlayerSlot& slot : slots_) {
            if (slot.occupied && slot.peer == peer) {
                return &slot;
            }
        }
        return nullptr;
    }

    [[nodiscard]] PlayerSlot const* findByPeer(PeerId peer) const noexcept {
        return const_cast<AuthorityRoster*>(this)->findByPeer(peer);
    }

    std::array<PlayerSlot, MaxPlayers> slots_{};
    std::size_t connectedCount_{};
    MultiplayerMode mode_{MultiplayerMode::Offline};
};

struct InputCommand {
    PeerId peer{kInvalidPeerId};
    std::uint32_t tick{};
    std::int16_t moveX{};
    std::int16_t moveY{};
    std::uint16_t buttons{};
};

template <std::size_t Capacity>
class InputCommandQueue {
public:
    static_assert(Capacity > 0, "InputCommandQueue requires positive capacity");

    [[nodiscard]] bool push(InputCommand const& cmd) noexcept {
        if (cmd.peer == kInvalidPeerId || count_ >= Capacity) {
            return false;
        }
        queue_[count_++] = cmd;
        return true;
    }

    [[nodiscard]] std::size_t drainTick(std::uint32_t tick, InputCommand* out, std::size_t outCapacity) noexcept {
        if (out == nullptr || outCapacity == 0u) {
            return 0u;
        }
        std::size_t n = 0u;
        std::size_t write = 0u;
        for (std::size_t i = 0u; i < count_; ++i) {
            InputCommand const cmd = queue_[i];
            if (cmd.tick == tick) {
                if (n < outCapacity) {
                    out[n] = cmd;
                }
                ++n;
            } else {
                queue_[write++] = cmd;
            }
        }
        count_ = write;
        return n;
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }

private:
    std::array<InputCommand, Capacity> queue_{};
    std::size_t count_{};
};

enum class ReplicationChannel : std::uint8_t {
    Control,
    Inputs,
    StateDelta
};

enum class ReplicationReliability : std::uint8_t {
    Unreliable,
    Reliable
};

struct ReplicationMessageMeta {
    ReplicationChannel channel{ReplicationChannel::StateDelta};
    ReplicationReliability reliability{ReplicationReliability::Unreliable};
    std::uint32_t tick{};
    std::uint32_t sequence{};
};

[[nodiscard]] constexpr bool requiresAck(ReplicationMessageMeta const& m) noexcept {
    return m.reliability == ReplicationReliability::Reliable || m.channel == ReplicationChannel::Control;
}

} // namespace marble::gameplay
