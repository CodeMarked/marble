#include "gameplay/UdpGameTransport.hpp"

#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace marble::gameplay {

bool parseIpv4Host(char const* host, std::uint32_t& outIpv4Network) noexcept {
    if (host == nullptr) {
        return false;
    }
    in_addr addr{};
#if defined(_WIN32)
    if (InetPtonA(AF_INET, host, &addr) != 1) {
        return false;
    }
#else
    if (inet_pton(AF_INET, host, &addr) != 1) {
        return false;
    }
#endif
    outIpv4Network = addr.s_addr;
    return true;
}

UdpGameTransport::~UdpGameTransport() {
    shutdown();
}

bool UdpGameTransport::bind(std::uint16_t port) noexcept {
    shutdown();
    if (!socket_.open()) {
        return false;
    }
    if (!socket_.setNonBlocking(true)) {
        shutdown();
        return false;
    }
    if (!socket_.bindPort(port)) {
        shutdown();
        return false;
    }
    bound_ = true;
    return true;
}

void UdpGameTransport::shutdown() noexcept {
    socket_.close();
    bound_ = false;
    for (PeerEntry& e : peers_) {
        e = {};
    }
}

UdpGameTransport::PeerEntry* UdpGameTransport::findPeerById(PeerId id) noexcept {
    for (PeerEntry& e : peers_) {
        if (e.active && e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

UdpGameTransport::PeerEntry const* UdpGameTransport::findPeerById(PeerId id) const noexcept {
    for (PeerEntry const& e : peers_) {
        if (e.active && e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

PeerId UdpGameTransport::peerIdFromSource(std::uint32_t ipv4Network, std::uint16_t portHost) const noexcept {
    for (PeerEntry const& e : peers_) {
        if (e.active && e.ipv4Network == ipv4Network && e.port == portHost) {
            return e.id;
        }
    }
    return kInvalidPeerId;
}

bool UdpGameTransport::addPeerEndpoint(PeerId id, std::uint32_t ipv4Network, std::uint16_t portHost) noexcept {
    if (id == kInvalidPeerId) {
        return false;
    }
    PeerEntry* slot = findPeerById(id);
    if (slot == nullptr) {
        for (PeerEntry& e : peers_) {
            if (!e.active) {
                slot = &e;
                break;
            }
        }
    }
    if (slot == nullptr) {
        return false;
    }
    slot->id = id;
    slot->ipv4Network = ipv4Network;
    slot->port = portHost;
    slot->active = true;
    return true;
}

bool UdpGameTransport::addPeer(PeerId id, char const* ipv4Host, std::uint16_t port) noexcept {
    if (id == kInvalidPeerId || ipv4Host == nullptr) {
        return false;
    }
    std::uint32_t net{};
    if (!parseIpv4Host(ipv4Host, net)) {
        return false;
    }
    return addPeerEndpoint(id, net, port);
}

bool UdpGameTransport::sendRaw(
    std::uint32_t destIpv4Network,
    std::uint16_t destPortHost,
    void const* data,
    std::size_t len
) noexcept {
    if (!bound_ || data == nullptr || len == 0u) {
        return false;
    }
    return socket_.sendTo(data, len, destIpv4Network, destPortHost);
}

std::size_t UdpGameTransport::receiveRaw(
    std::uint32_t& outSrcIpv4Network,
    std::uint16_t& outSrcPortHost,
    void* buffer,
    std::size_t bufferBytes
) noexcept {
    if (!bound_ || buffer == nullptr || bufferBytes == 0u) {
        return 0u;
    }
    std::size_t len = 0u;
    if (!socket_.tryRecvFrom(buffer, bufferBytes, len, outSrcIpv4Network, outSrcPortHost)) {
        return 0u;
    }
    return len;
}

bool UdpGameTransport::send(PeerId to, void const* data, std::size_t len) noexcept {
    if (!bound_ || data == nullptr || len == 0u) {
        return false;
    }
    PeerEntry const* const p = findPeerById(to);
    if (p == nullptr) {
        return false;
    }
    return socket_.sendTo(data, len, p->ipv4Network, p->port);
}

std::size_t UdpGameTransport::receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept {
    if (!bound_ || buffer == nullptr || bufferBytes == 0u) {
        return 0u;
    }
    std::size_t len = 0u;
    std::uint32_t addr = 0u;
    std::uint16_t port = 0u;
    if (!socket_.tryRecvFrom(buffer, bufferBytes, len, addr, port)) {
        return 0u;
    }
    PeerId const from = peerIdFromSource(addr, port);
    if (from == kInvalidPeerId) {
        return 0u;
    }
    outFrom = from;
    return len;
}

} // namespace marble::gameplay
