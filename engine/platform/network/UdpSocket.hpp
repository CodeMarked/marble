#pragma once

#include <cstddef>
#include <cstdint>

namespace marble::platform::network {

/// Increment Winsock refcount (Windows only). Safe to call multiple times; pair with [`releaseNetworking`].
void acquireNetworking() noexcept;

/// Decrement Winsock refcount (Windows only). Last release calls `WSACleanup`.
void releaseNetworking() noexcept;

/// Non-blocking UDP/IPv4 datagram socket. Bound address is IPv4.
class UdpSocket {
public:
    UdpSocket() noexcept = default;
    UdpSocket(UdpSocket const&) = delete;
    UdpSocket& operator=(UdpSocket const&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    ~UdpSocket();

    [[nodiscard]] bool isOpen() const noexcept { return handle_ != kInvalidHandle; }

    /// Create the UDP socket. On Windows, ensures Winsock is acquired.
    [[nodiscard]] bool open() noexcept;

    void close() noexcept;

    /// Bind to `port` (host order). Use `0` for an ephemeral port. IPv4 any.
    [[nodiscard]] bool bindPort(std::uint16_t port) noexcept;

    [[nodiscard]] bool setNonBlocking(bool enabled) noexcept;

    /// Local bound port (host order). Valid after successful `bindPort`.
    [[nodiscard]] std::uint16_t localPort() const noexcept;

    /// `ipv4Addr` is IPv4 **network-endian** (`in_addr::s_addr`). `port` is host order.
    [[nodiscard]] bool sendTo(
        void const* data,
        std::size_t len,
        std::uint32_t ipv4Addr,
        std::uint16_t port
    ) noexcept;

    /// Returns `true` if a datagram was read. `outIpv4Addr` network-endian, `outPort` host order.
    [[nodiscard]] bool tryRecvFrom(
        void* buffer,
        std::size_t bufferBytes,
        std::size_t& outLen,
        std::uint32_t& outIpv4Addr,
        std::uint16_t& outPort
    ) noexcept;

    /// Platform error code for the last failure (e.g. `WSAGetLastError` / `errno`).
    [[nodiscard]] int lastSystemError() const noexcept { return lastError_; }

    [[nodiscard]] static bool errorIsWouldBlock(int code) noexcept;

private:
#if defined(_WIN32)
    using Handle = std::uintptr_t;
    static constexpr Handle kInvalidHandle = static_cast<Handle>(~static_cast<Handle>(0));
#else
    using Handle = int;
    static constexpr Handle kInvalidHandle = -1;
#endif

    Handle handle_{kInvalidHandle};
    std::uint16_t boundPort_{};
    int lastError_{};
};

} // namespace marble::platform::network
