#include "platform/network/UdpSocket.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace marble::platform::network {

#if defined(_WIN32)

namespace {

std::int32_t g_wsaRefs = 0;

[[nodiscard]] SOCKET toSocket(std::uintptr_t h) noexcept {
    return static_cast<SOCKET>(h);
}

[[nodiscard]] std::uintptr_t fromSocket(SOCKET s) noexcept {
    return static_cast<std::uintptr_t>(s);
}

} // namespace

void acquireNetworking() noexcept {
    if (g_wsaRefs++ == 0) {
        WSADATA wsaData{};
        (void)WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
}

void releaseNetworking() noexcept {
    if (g_wsaRefs <= 0) {
        return;
    }
    if (--g_wsaRefs == 0) {
        WSACleanup();
    }
}

#else

void acquireNetworking() noexcept {}

void releaseNetworking() noexcept {}

#endif

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : handle_(other.handle_)
    , boundPort_(other.boundPort_)
    , lastError_(other.lastError_) {
    other.handle_ = kInvalidHandle;
    other.boundPort_ = 0;
    other.lastError_ = 0;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        boundPort_ = other.boundPort_;
        lastError_ = other.lastError_;
        other.handle_ = kInvalidHandle;
        other.boundPort_ = 0;
        other.lastError_ = 0;
    }
    return *this;
}

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::open() noexcept {
    close();
#if defined(_WIN32)
    acquireNetworking();
    SOCKET const s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        lastError_ = static_cast<int>(WSAGetLastError());
        releaseNetworking();
        return false;
    }
    handle_ = fromSocket(s);
    lastError_ = 0;
    return true;
#else
    int const s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        lastError_ = errno;
        return false;
    }
    handle_ = s;
    lastError_ = 0;
    return true;
#endif
}

void UdpSocket::close() noexcept {
    if (handle_ == kInvalidHandle) {
        return;
    }
#if defined(_WIN32)
    closesocket(toSocket(handle_));
    releaseNetworking();
#else
    (void)::close(handle_);
#endif
    handle_ = kInvalidHandle;
    boundPort_ = 0;
    lastError_ = 0;
}

bool UdpSocket::bindPort(std::uint16_t port) noexcept {
    if (!isOpen()) {
        lastError_ = static_cast<int>(EINVAL);
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
#if defined(_WIN32)
    if (bind(toSocket(handle_), reinterpret_cast<sockaddr*>(&addr), static_cast<int>(sizeof(addr))) == SOCKET_ERROR) {
        lastError_ = static_cast<int>(WSAGetLastError());
        return false;
    }
#else
    if (bind(handle_, reinterpret_cast<sockaddr*>(&addr), static_cast<int>(sizeof(addr))) < 0) {
        lastError_ = errno;
        return false;
    }
#endif
    sockaddr_in out{};
    socklen_t len = static_cast<socklen_t>(sizeof(out));
#if defined(_WIN32)
    if (getsockname(toSocket(handle_), reinterpret_cast<sockaddr*>(&out), &len) == SOCKET_ERROR) {
        lastError_ = static_cast<int>(WSAGetLastError());
        return false;
    }
#else
    if (getsockname(handle_, reinterpret_cast<sockaddr*>(&out), &len) < 0) {
        lastError_ = errno;
        return false;
    }
#endif
    boundPort_ = ntohs(out.sin_port);
    lastError_ = 0;
    return true;
}

bool UdpSocket::setNonBlocking(bool enabled) noexcept {
    if (!isOpen()) {
        lastError_ = static_cast<int>(EINVAL);
        return false;
    }
#if defined(_WIN32)
    u_long mode = enabled ? 1UL : 0UL;
    if (ioctlsocket(toSocket(handle_), FIONBIO, &mode) == SOCKET_ERROR) {
        lastError_ = static_cast<int>(WSAGetLastError());
        return false;
    }
#else
    int flags = fcntl(handle_, F_GETFL, 0);
    if (flags < 0) {
        lastError_ = errno;
        return false;
    }
    if (enabled) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (fcntl(handle_, F_SETFL, flags) < 0) {
        lastError_ = errno;
        return false;
    }
#endif
    lastError_ = 0;
    return true;
}

std::uint16_t UdpSocket::localPort() const noexcept {
    return boundPort_;
}

bool UdpSocket::sendTo(
    void const* data,
    std::size_t len,
    std::uint32_t ipv4Addr,
    std::uint16_t port
) noexcept {
    if (!isOpen() || data == nullptr || len == 0u) {
        lastError_ = static_cast<int>(EINVAL);
        return false;
    }
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    to.sin_addr.s_addr = ipv4Addr;
#if defined(_WIN32)
    int const r = sendto(
        toSocket(handle_),
        static_cast<char const*>(data),
        static_cast<int>(len),
        0,
        reinterpret_cast<sockaddr const*>(&to),
        static_cast<int>(sizeof(to))
    );
    if (r == SOCKET_ERROR) {
        lastError_ = static_cast<int>(WSAGetLastError());
        return false;
    }
#else
    ssize_t const r = sendto(
        handle_,
        data,
        len,
        0,
        reinterpret_cast<sockaddr const*>(&to),
        static_cast<int>(sizeof(to))
    );
    if (r < 0) {
        lastError_ = errno;
        return false;
    }
#endif
    lastError_ = 0;
    return true;
}

bool UdpSocket::tryRecvFrom(
    void* buffer,
    std::size_t bufferBytes,
    std::size_t& outLen,
    std::uint32_t& outIpv4Addr,
    std::uint16_t& outPort
) noexcept {
    outLen = 0;
    if (!isOpen() || buffer == nullptr || bufferBytes == 0u) {
        lastError_ = static_cast<int>(EINVAL);
        return false;
    }
    sockaddr_in from{};
    socklen_t fromLen = static_cast<socklen_t>(sizeof(from));
#if defined(_WIN32)
    int const r = recvfrom(
        toSocket(handle_),
        static_cast<char*>(buffer),
        static_cast<int>(bufferBytes),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &fromLen
    );
    if (r == SOCKET_ERROR) {
        int const e = static_cast<int>(WSAGetLastError());
        lastError_ = e;
        if (e == WSAEWOULDBLOCK || e == WSAECONNRESET) {
            return false;
        }
        return false;
    }
#else
    ssize_t const r = recvfrom(
        handle_,
        buffer,
        bufferBytes,
        0,
        reinterpret_cast<sockaddr*>(&from),
        &fromLen
    );
    if (r < 0) {
        lastError_ = errno;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        return false;
    }
#endif
    outLen = static_cast<std::size_t>(r);
    outIpv4Addr = from.sin_addr.s_addr;
    outPort = ntohs(from.sin_port);
    lastError_ = 0;
    return true;
}

bool UdpSocket::errorIsWouldBlock(int code) noexcept {
#if defined(_WIN32)
    return code == WSAEWOULDBLOCK;
#else
    return code == EAGAIN || code == EWOULDBLOCK;
#endif
}

} // namespace marble::platform::network
