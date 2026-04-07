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

    /// Next datagram from any source; does **not** require a peer table match. For handshake demux.
    [[nodiscard]] std::size_t receiveRaw(
        std::uint32_t& outSrcIpv4Network,
        std::uint16_t& outSrcPortHost,
        void* buffer,
        std::size_t bufferBytes
    ) noexcept;

    [[nodiscard]] bool send(PeerId to, void const* data, std::size_t len) noexcept override;

    [[nodiscard]] std::size_t receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept override;

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

    marble::platform::network::UdpSocket socket_{};
    std::array<PeerEntry, kMaxPeers> peers_{};
    bool bound_{};
};

} // namespace marble::gameplay
