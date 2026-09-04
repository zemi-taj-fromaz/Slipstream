//
// Created by babodev on 15.08.2026..
//

#ifndef SLIPSTREAM_SOCKET_H
#define SLIPSTREAM_SOCKET_H

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace utils {
    enum class ConnectionResult : std::uint8_t {
        Complete,
        PeerDisconnected
    };

    enum class SockType : std::uint8_t {
        Udp,
        Tcp
    };

    template <SockType type>
    class Socket {
    public:
        Socket();
        ~Socket();

        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;

        void SetReuseAddress(bool enabled = true);
        void SetReceiveBufferSize(int size);
        void SetSendBufferSize(int size);

        void Bind(std::uint16_t port);
        void Bind(const char* address, std::uint16_t port);

        void SetMulticastTtl(std::uint8_t ttl) requires (type == SockType::Udp);
        void SetMulticastLoop(bool enabled = true) requires (type == SockType::Udp);
        void SetMulticastInterface(const char* address) requires (type == SockType::Udp);
        void JoinMulticastGroup(
            const char* group,
            const char* interface_address = "0.0.0.0")
            requires (type == SockType::Udp);
        void SendDatagram(
            std::span<const std::byte> bytes,
            const char* address,
            std::uint16_t port)
            requires (type == SockType::Udp);
        ssize_t RecvDatagram(std::span<std::byte> buffer) requires (type == SockType::Udp);

        void SetTcpNoDelay(bool enabled = true) requires (type == SockType::Tcp);
        void Listen(int backlog = 8) requires (type == SockType::Tcp);
        [[nodiscard]] Socket Accept() requires (type == SockType::Tcp);
        void Connect(const char* address, std::uint16_t port) requires (type == SockType::Tcp);
        ConnectionResult SendAll(std::span<const std::byte> bytes) requires (type == SockType::Tcp);

        ssize_t Recv(std::span<std::byte> buffer) requires (type == SockType::Tcp);

        void Shutdown(int how);

        [[nodiscard]] int NativeHandle() const noexcept {
            return fd;
        }
        bool IsValid() const noexcept;
    private:
        void SetSockOption(int level, int option, int value);
        explicit Socket(int existing) noexcept;
        int fd{-1};
    };

    template <SockType type>
    Socket<type>::Socket() {
        int socket_type{};
        if constexpr (type == SockType::Tcp) {
            socket_type = SOCK_STREAM;
        } else if constexpr (type == SockType::Udp) {
            socket_type = SOCK_DGRAM;
        }

        fd = ::socket(AF_INET, socket_type | SOCK_CLOEXEC, 0);
        if (fd == -1) {
            throw std::runtime_error("socket() failed");
        }
    }

    template <SockType type>
    Socket<type>::~Socket() {
        if (IsValid()) {
            ::close(fd);
        }
    }

    template <SockType type>
    Socket<type>::Socket(Socket&& other) noexcept {
        fd = std::exchange(other.fd, -1);
    }

    template <SockType type>
    Socket<type>& Socket<type>::operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (IsValid()) {
                ::close(fd);
            }

            fd = std::exchange(other.fd, -1);
        }

        return *this;
    }

    template <SockType type>
    void Socket<type>::SetReuseAddress(bool enabled) {
        SetSockOption(SOL_SOCKET, SO_REUSEADDR, enabled ? 1 : 0);
    }

    template <SockType type>
    void Socket<type>::SetReceiveBufferSize(int size) {
        if (size <= 0) {
            throw std::invalid_argument("receive buffer size must be positive");
        }

        SetSockOption(SOL_SOCKET, SO_RCVBUF, size);
    }

    template <SockType type>
    void Socket<type>::SetSendBufferSize(int size) {
        if (size <= 0) {
            throw std::invalid_argument("send buffer size must be positive");
        }

        SetSockOption(SOL_SOCKET, SO_SNDBUF, size);
    }

    template <SockType type>
    void Socket<type>::Bind(std::uint16_t port) {
        Bind("0.0.0.0", port);
    }

    template <SockType type>
    void Socket<type>::Bind(const char* address, std::uint16_t port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        const int conversion_result =
            ::inet_pton(AF_INET, address, &addr.sin_addr);
        if (conversion_result == 0) {
            throw std::invalid_argument("invalid IPv4 bind address");
        }
        if (conversion_result == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "inet_pton() failed for bind address");
        }

        const int result = ::bind(
            fd,
            reinterpret_cast<const sockaddr*>(&addr),
            sizeof(addr));
        if (result == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "bind() failed");
        }
    }

    template <SockType type>
    void Socket<type>::SetMulticastTtl(std::uint8_t ttl)
        requires (type == SockType::Udp) {
        SetSockOption(IPPROTO_IP, IP_MULTICAST_TTL, static_cast<int>(ttl));
    }

    template <SockType type>
    void Socket<type>::SetMulticastLoop(bool enabled)
        requires (type == SockType::Udp) {
        SetSockOption(IPPROTO_IP, IP_MULTICAST_LOOP, enabled ? 1 : 0);
    }

    template <SockType type>
    void Socket<type>::SetMulticastInterface(const char* address)
        requires (type == SockType::Udp) {
        in_addr interface_address{};
        const int conversion_result =
            ::inet_pton(AF_INET, address, &interface_address);
        if (conversion_result == 0) {
            throw std::invalid_argument("invalid IPv4 multicast interface address");
        }
        if (conversion_result == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "inet_pton() failed for multicast interface address");
        }

        const int result = ::setsockopt(
            fd,
            IPPROTO_IP,
            IP_MULTICAST_IF,
            &interface_address,
            sizeof(interface_address));
        if (result == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "setsockopt(IP_MULTICAST_IF) failed");
        }
    }

    template <SockType type>
    void Socket<type>::JoinMulticastGroup(
        const char* group,
        const char* interface_address)
        requires (type == SockType::Udp) {
        ip_mreq membership{};

        const int group_conversion =
            ::inet_pton(AF_INET, group, &membership.imr_multiaddr);
        if (group_conversion == 0) {
            throw std::invalid_argument("invalid IPv4 multicast group address");
        }
        if (group_conversion == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "inet_pton() failed for multicast group address");
        }
        if (!IN_MULTICAST(ntohl(membership.imr_multiaddr.s_addr))) {
            throw std::invalid_argument("address is not an IPv4 multicast group");
        }

        const int interface_conversion =
            ::inet_pton(AF_INET, interface_address, &membership.imr_interface);
        if (interface_conversion == 0) {
            throw std::invalid_argument("invalid IPv4 multicast interface address");
        }
        if (interface_conversion == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "inet_pton() failed for multicast interface address");
        }

        const int result = ::setsockopt(
            fd,
            IPPROTO_IP,
            IP_ADD_MEMBERSHIP,
            &membership,
            sizeof(membership));
        if (result == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "setsockopt(IP_ADD_MEMBERSHIP) failed");
        }
    }

    template <SockType type>
    void Socket<type>::SendDatagram(
        std::span<const std::byte> bytes,
        const char* address,
        std::uint16_t port)
        requires (type == SockType::Udp) {
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_port = htons(port);

        const int conversion_result =
            ::inet_pton(AF_INET, address, &destination.sin_addr);
        if (conversion_result == 0) {
            throw std::invalid_argument("invalid IPv4 destination address");
        }
        if (conversion_result == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "inet_pton() failed for destination address");
        }

        while (true) {
            const ssize_t sent = ::sendto(
                fd,
                bytes.data(),
                bytes.size(),
                MSG_DONTWAIT | MSG_NOSIGNAL,
                reinterpret_cast<const sockaddr*>(&destination),
                sizeof(destination));

            if (sent >= 0) {
                if (static_cast<std::size_t>(sent) != bytes.size()) {
                    throw std::runtime_error("sendto() sent a partial datagram");
                }
                return;
            }

            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }

            throw std::system_error(
                errno,
                std::generic_category(),
                "sendto() failed");
        }
    }

    template <SockType type>
    void Socket<type>::SetTcpNoDelay(bool enabled) requires (type == SockType::Tcp) {
        SetSockOption(IPPROTO_TCP, TCP_NODELAY, enabled ? 1 : 0);
    }

    template <SockType type>
    void Socket<type>::Listen(int backlog) requires (type == SockType::Tcp) {
        const int result = ::listen(fd, backlog);
        if (result == -1) {
            throw std::runtime_error("listen() failed");
        }
    }

    template <SockType type>
    Socket<type> Socket<type>::Accept() requires (type == SockType::Tcp) {
        const int connected_fd = ::accept(fd, nullptr, nullptr);
        if (connected_fd == -1) {
            throw std::runtime_error("accept() failed");
        }

        return Socket(connected_fd);
    }

    template <SockType type>
    void Socket<type>::Connect(const char* address, std::uint16_t port) requires (type == SockType::Tcp) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        const int result = ::inet_pton(AF_INET, address, &addr.sin_addr);
        if (result == 0) {
            throw std::invalid_argument("inet_pton() failed");
        }
        if (result == -1) {
            throw std::runtime_error("inet_pton() failed");
        }

        const int connection_result = ::connect(
            fd,
            reinterpret_cast<const sockaddr*>(&addr),
            sizeof(addr));
        if (connection_result == -1) {
            throw std::runtime_error("connect() failed");
        }
    }

    template <SockType type>
    ssize_t Socket<type>::Recv(std::span<std::byte> buffer) requires (type == SockType::Tcp) {
        return ::recv(fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
    }

    template <SockType type>
    ssize_t Socket<type>::RecvDatagram(std::span<std::byte> buffer) requires (type == SockType::Udp) {
        return ::recv(fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
    }

    template <SockType type>
    ConnectionResult Socket<type>::SendAll(std::span<const std::byte> bytes)
        requires (type == SockType::Tcp) {
        std::size_t offset{};

        while (offset < bytes.size()) {
            const ssize_t sent = ::send(
                fd,
                bytes.data() + offset,
                bytes.size() - offset,
                MSG_DONTWAIT | MSG_NOSIGNAL);

            if (sent > 0) {
                offset += static_cast<std::size_t>(sent);
                continue;
            }

            if (sent == 0) {
                return ConnectionResult::PeerDisconnected;
            }

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Deliberately busy-spin until send-buffer space exists.
            }

            if (errno == EPIPE ||
                errno == ECONNRESET ||
                errno == ENOTCONN) {
                return ConnectionResult::PeerDisconnected;
            }

            throw std::system_error(
                errno,
                std::generic_category(),
                "send failed");
        }

        return ConnectionResult::Complete;
    }

    template <SockType type>
    void Socket<type>::Shutdown(int how) {
        const int result = ::shutdown(fd, how);
        if (result == -1) {
            throw std::runtime_error("shutdown() failed");
        }
    }

    template <SockType type>
    bool Socket<type>::IsValid() const noexcept {
        return fd != -1;
    }

    template <SockType type>
    void Socket<type>::SetSockOption(int level, int option, int value) {
        const int result = ::setsockopt(
            fd,
            level,
            option,
            &value,
            sizeof(value));

        if (result == -1) {
            throw std::runtime_error("setsockopt failed");
        }
    }

    template <SockType type>
    Socket<type>::Socket(int existing) noexcept {
        fd = existing;
    }

    using TcpSocket = Socket<SockType::Tcp>;
    using UdpSocket = Socket<SockType::Udp>;
}

#endif //SLIPSTREAM_SOCKET_H
