#pragma once

#include "gameplay/GameTransport.hpp"
#include "platform/network/UdpSocket.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

/// Parse dotted IPv4 (e.g. `127.0.0.1`) into network-endian `s_addr`. Allocation-free.
[[nodiscard]] bool parseIpv4Host(char const* host, std::uint32_t& outIpv4Network) noexcept;

/// UDP/IPv4 `IGameTransport` with a fixed peer table (addresses known up front).
class UdpGameTransport final : public IGameTransport {
public:
    static constexpr std::size_t kMaxPeers = 16;

    UdpGameTransport() noexcept = default;
    ~UdpGameTransport() override;

    UdpGameTransport(UdpGameTransport const&) = delete;
    UdpGameTransport& operator=(UdpGameTransport const&) = delete;

    UdpGameTransport(UdpGameTransport&&) noexcept = delete;
    UdpGameTransport& operator=(UdpGameTransport&&) noexcept = delete;

    /// Open socket, non-blocking bind. `port` 0 chooses an ephemeral local port.
    [[nodiscard]] bool bind(std::uint16_t port) noexcept;

    void shutdown() noexcept;

    /// Register `id` at UDP `ipv4Host`:`port` (host order port). Replaces an existing row with the same `id`.
    [[nodiscard]] bool addPeer(PeerId id, char const* ipv4Host, std::uint16_t port) noexcept;

    /// Same as [`addPeer`] but with `ipv4Network` as `in_addr::s_addr` (network byte order).
    [[nodiscard]] bool addPeerEndpoint(PeerId id, std::uint32_t ipv4Network, std::uint16_t portHost) noexcept;

    /// Send without a peer row (e.g. server reply before [`addPeer`]). `ipv4Network` = `in_addr::s_addr`.
    [[nodiscard]] bool sendRaw(
        std::uint32_t destIpv4Network,
        std::uint16_t destPortHost,
        void const* data,
        std::size_t len
    ) noexcept;

    [[nodiscard]] bool send(PeerId to, void const* data, std::size_t len) noexcept override;

    [[nodiscard]] std::size_t receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept override;

    void forgetPeer(PeerId peer) noexcept override;

    /// Drain pending UDP datagrams into internal queues and assign peer IDs for unknown sources that send
    /// a session Hello envelope. Optional explicit call before a batch of `receive`; `receive` pumps as well.
    void pumpIngress() noexcept;

    /// First auto-assigned joiner id when an unknown host sends Hello (default **2**). Listen hosts that reserve
    /// peer **2** for synthetic local input should set this to **3** after [`bind`].
    void setMinimumJoinerPeerId(PeerId id) noexcept;

    [[nodiscard]] PeerId minimumJoinerPeerId() const noexcept { return minJoinerPeerId_; }

    [[nodiscard]] std::uint16_t localPort() const noexcept { return socket_.localPort(); }

    [[nodiscard]] bool isBound() const noexcept { return bound_; }

private:
    struct PeerEntry {
        PeerId id{kInvalidPeerId};
        std::uint32_t ipv4Network{};
        std::uint16_t port{};
        bool active{};
    };

    [[nodiscard]] PeerEntry* findPeerById(PeerId id) noexcept;
    [[nodiscard]] PeerEntry const* findPeerById(PeerId id) const noexcept;
    [[nodiscard]] PeerId peerIdFromSource(std::uint32_t ipv4Network, std::uint16_t portHost) const noexcept;
    [[nodiscard]] PeerId allocateJoinerPeerId() const noexcept;
    [[nodiscard]] bool hasFreePeerSlot() const noexcept;
    [[nodiscard]] bool tryEnqueueIngress(PeerId from, void const* data, std::size_t len) noexcept;
    [[nodiscard]] std::size_t dequeueIngress(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept;
    void purgeIngressForPeer(PeerId peer) noexcept;

    static constexpr std::size_t kMaxDatagramBytes = 2048u;
    static constexpr std::size_t kIngressQueueDepth = 48u;

    struct IngressEntry {
        PeerId from{kInvalidPeerId};
        std::uint16_t len{};
        std::array<std::uint8_t, kMaxDatagramBytes> bytes{};
    };

    marble::platform::network::UdpSocket socket_{};
    std::array<PeerEntry, kMaxPeers> peers_{};
    std::array<IngressEntry, kIngressQueueDepth> ingressQueue_{};
    std::size_t ingressHead_{};
    std::size_t ingressCount_{};
    bool bound_{};
    /// Reset to **2** on each successful [`bind`].
    PeerId minJoinerPeerId_{2u};
};

} // namespace marble::gameplay
