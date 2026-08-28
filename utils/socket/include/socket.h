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
        void SetTcpNoDelay(bool enabled = true);
        void Bind(std::uint16_t port);
        void Bind(const char* address, std::uint16_t port);
        void Listen(int backlog = 8); // waits for connection
        [[nodiscard]]
        Socket Accept();

        void Connect(const char* address, std::uint16_t port);

        ssize_t Recv(std::span<std::byte> buffer);
        ConnectionResult SendAll(std::span<const std::byte> bytes);
        void Shutdown(int how);

        [[nodiscard]]
        int NativeHandle() const noexcept {
            return fd;
        }

        bool IsValid() const noexcept;
    private:
        void SetSockOption(int level, int option, int value);
        explicit Socket(int existing) noexcept;
        int fd{-1};
    };

    inline Socket::Socket() {
        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd == -1) {
            throw std::runtime_error("socket() failed");
        }
    }

    inline Socket::~Socket() {
        if (IsValid()) {
            ::close(fd);
        }
    }

    inline Socket::Socket(Socket&& other) noexcept {
        fd = std::exchange(other.fd, -1);
    }

    inline Socket& Socket::operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (IsValid()) {
                ::close(fd);
            }

            fd = std::exchange(other.fd, -1);
        }

        return *this;
    }

    inline void Socket::Bind(std::uint16_t port) {
        Bind("0.0.0.0", port);
    }

    inline void Socket::Bind(const char* address, std::uint16_t port) {
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

    inline void Socket::SetSockOption(int level, int option, int value) {
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

    inline void Socket::SetReuseAddress(bool enabled) {
        SetSockOption(SOL_SOCKET, SO_REUSEADDR, enabled ? 1 : 0);
    }

    inline void Socket::SetReceiveBufferSize(int size) {
        if (size <= 0) {
            throw std::invalid_argument("receive buffer size must be positive");
        }

        SetSockOption(SOL_SOCKET, SO_RCVBUF, size);
    }

    inline void Socket::SetSendBufferSize(int size) {
        if (size <= 0) {
            throw std::invalid_argument("send buffer size must be positive");
        }

        SetSockOption(SOL_SOCKET, SO_SNDBUF, size);
    }

    inline void Socket::SetTcpNoDelay(bool enabled) {
        SetSockOption(IPPROTO_TCP, TCP_NODELAY, enabled ? 1 : 0);
    }

    inline void Socket::Listen(int backlog) {
        const int result = ::listen(fd, backlog);
        if (result == -1) {
            throw std::runtime_error("bind() failed");
        }
    }

    inline Socket Socket::Accept() {
        const int connected_fd = ::accept(fd, nullptr, nullptr);
        if (connected_fd == -1) {
            throw std::runtime_error("accept() failed");
        }

        return Socket(connected_fd);
    }

    inline void Socket::Connect(const char* address, std::uint16_t port) {
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

    inline ssize_t Socket::Recv(std::span<std::byte> buffer) {
        return ::recv(fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
    }

    inline ConnectionResult Socket::SendAll(std::span<const std::byte> bytes) {
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

    inline void Socket::Shutdown(int how) {
        const int result = ::shutdown(fd, how);
        if (result == -1) {
            throw std::runtime_error("shutdown() failed");
        }
    }

    inline bool Socket::IsValid() const noexcept {
        return fd != -1;
    }

    inline Socket::Socket(int existing) noexcept {
        fd = existing;
    }
}

#endif //SLIPSTREAM_SOCKET_H