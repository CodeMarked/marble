#include "gameplay/UdpGameTransport.hpp"

#include "gameplay/MultiplayerSessionEnvelope.hpp"

#include <algorithm>
#include <array>
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
    minJoinerPeerId_ = 2u;
    bound_ = true;
    ingressHead_ = 0u;
    ingressCount_ = 0u;
    for (IngressEntry& q : ingressQueue_) {
        q = {};
    }
    return true;
}

void UdpGameTransport::shutdown() noexcept {
    socket_.close();
    bound_ = false;
    for (PeerEntry& e : peers_) {
        e = {};
    }
    ingressHead_ = 0u;
    ingressCount_ = 0u;
    for (IngressEntry& q : ingressQueue_) {
        q = {};
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

bool UdpGameTransport::hasFreePeerSlot() const noexcept {
    std::size_t active = 0u;
    for (PeerEntry const& e : peers_) {
        if (e.active) {
            ++active;
        }
    }
    return active < kMaxPeers;
}

void UdpGameTransport::setMinimumJoinerPeerId(PeerId id) noexcept {
    minJoinerPeerId_ = id < 2u ? 2u : id;
}

PeerId UdpGameTransport::allocateJoinerPeerId() const noexcept {
    if (!hasFreePeerSlot()) {
        return kInvalidPeerId;
    }
    PeerId const start = minJoinerPeerId_ < 2u ? 2u : minJoinerPeerId_;
    for (PeerId cand = start; cand < 500u; ++cand) {
        if (findPeerById(cand) == nullptr) {
            return cand;
        }
    }
    return kInvalidPeerId;
}

bool UdpGameTransport::tryEnqueueIngress(PeerId from, void const* data, std::size_t len) noexcept {
    if (from == kInvalidPeerId || data == nullptr || len == 0u || len > kMaxDatagramBytes) {
        return false;
    }
    if (ingressCount_ >= kIngressQueueDepth) {
        return false;
    }
    std::size_t const idx = (ingressHead_ + ingressCount_) % kIngressQueueDepth;
    IngressEntry& e = ingressQueue_[idx];
    e.from = from;
    e.len = static_cast<std::uint16_t>(len);
    std::memcpy(e.bytes.data(), data, len);
    ++ingressCount_;
    return true;
}

std::size_t UdpGameTransport::dequeueIngress(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept {
    if (ingressCount_ == 0u || buffer == nullptr || bufferBytes == 0u) {
        return 0u;
    }
    IngressEntry const& e = ingressQueue_[ingressHead_];
    std::size_t const n = std::min<std::size_t>(static_cast<std::size_t>(e.len), bufferBytes);
    std::memcpy(buffer, e.bytes.data(), n);
    outFrom = e.from;
    ingressHead_ = (ingressHead_ + 1u) % kIngressQueueDepth;
    --ingressCount_;
    return n;
}

void UdpGameTransport::pumpIngress() noexcept {
    if (!bound_) {
        return;
    }
    for (;;) {
        std::array<std::uint8_t, kMaxDatagramBytes> scratch{};
        std::uint32_t addr{};
        std::uint16_t port{};
        std::size_t len = 0u;
        if (!socket_.tryRecvFrom(scratch.data(), scratch.size(), len, addr, port)) {
            return;
        }
        if (len == 0u || len > kMaxDatagramBytes) {
            continue;
        }
        PeerId const known = peerIdFromSource(addr, port);
        if (known != kInvalidPeerId) {
            static_cast<void>(tryEnqueueIngress(known, scratch.data(), len));
            continue;
        }
        SessionMessageType msgType{};
        std::uint8_t flags{};
        std::uint8_t const* payload{};
        std::size_t payloadLen{};
        if (!parseSessionEnvelopeEx(scratch.data(), len, msgType, flags, payload, payloadLen) ||
            msgType != SessionMessageType::Hello) {
            continue;
        }
        PeerId const assign = allocateJoinerPeerId();
        if (assign == kInvalidPeerId) {
            continue;
        }
        if (!addPeerEndpoint(assign, addr, port)) {
            continue;
        }
        static_cast<void>(tryEnqueueIngress(assign, scratch.data(), len));
    }
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

void UdpGameTransport::purgeIngressForPeer(PeerId peer) noexcept {
    if (peer == kInvalidPeerId || ingressCount_ == 0u) {
        return;
    }
    std::array<IngressEntry, kIngressQueueDepth> kept{};
    std::size_t n = 0u;
    for (std::size_t i = 0u; i < ingressCount_; ++i) {
        std::size_t const idx = (ingressHead_ + i) % kIngressQueueDepth;
        if (ingressQueue_[idx].from != peer && n < kIngressQueueDepth) {
            kept[n] = ingressQueue_[idx];
            ++n;
        }
    }
    for (std::size_t i = 0u; i < n; ++i) {
        ingressQueue_[i] = kept[i];
    }
    ingressHead_ = 0u;
    ingressCount_ = n;
}

void UdpGameTransport::forgetPeer(PeerId peer) noexcept {
    if (peer == kInvalidPeerId) {
        return;
    }
    PeerEntry* const e = findPeerById(peer);
    if (e != nullptr) {
        e->active = false;
        e->id = kInvalidPeerId;
        e->ipv4Network = 0u;
        e->port = 0u;
    }
    purgeIngressForPeer(peer);
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
    pumpIngress();
    return dequeueIngress(outFrom, buffer, bufferBytes);
}

} // namespace marble::gameplay
