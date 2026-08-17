//
// Created by babodev on 15.08.2026..
//
#include "socket.h"
#include <string_view>
#include <utility>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>
#include <stdexcept>

namespace utils {

    Socket::Socket() {
        fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd == -1) { throw std::runtime_error("socket() failed"); }
    }

    Socket::~Socket() {
        if (IsValid()) {
            ::close(fd);
        }
    }

    Socket::Socket(Socket&& other) noexcept {
        fd = std::exchange(other.fd, -1);
    };


    void Socket::Bind(std::uint16_t port) {
        Bind("0.0.0.0", port);
    }

    void Socket::Bind(const char* address, std::uint16_t port) {
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

        const int result  = ::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        if (result == -1) {
            throw std::system_error(
                errno,
                std::generic_category(),
                "bind() failed");
        }
    }

    void Socket::SetSockOption(int level, int option, int value) {
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

    void Socket::SetReuseAddress(bool enabled) {
        SetSockOption(SOL_SOCKET, SO_REUSEADDR, enabled ? 1 : 0);
    }

    void Socket::SetReceiveBufferSize(int size) {
        if (size <= 0) {
            throw std::invalid_argument("receive buffer size must be positive");
        }

        SetSockOption(SOL_SOCKET, SO_RCVBUF, size);
    }

    void Socket::SetSendBufferSize(int size) {
        if (size <= 0) {
            throw std::invalid_argument("send buffer size must be positive");
        }

        SetSockOption(SOL_SOCKET, SO_SNDBUF, size);
    }

    void Socket::SetTcpNoDelay(bool enabled) {
        SetSockOption(IPPROTO_TCP, TCP_NODELAY, enabled ? 1 : 0);
    }

    void Socket::Listen(int backlog) {
        const int result  = ::listen(fd, backlog);
        if (result == -1) { throw std::runtime_error("bind() failed"); }
    }


    Socket Socket::Accept() {
        const int connected_fd = ::accept(fd, nullptr, nullptr);
        if (connected_fd == -1) { throw std::runtime_error("accept() failed"); }
        return Socket(connected_fd);
    }

    void Socket::Connect(const char* address, std::uint16_t port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        const int result = ::inet_pton(AF_INET, address, &addr.sin_addr);
        if (result == 0) { throw std::invalid_argument("inet_pton() failed"); }
        if (result == -1) { throw std::runtime_error("inet_pton() failed"); }

        const int connection_result = ::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        if (connection_result == -1) { throw std::runtime_error("connect() failed"); }
    }

    ssize_t Socket::Recv(std::span<std::byte> buffer) {
        return ::recv(fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
    };

    void Socket::SendAll(std::span<const std::byte> bytes) {

        std::size_t offset{};

        while (offset < bytes.size()) {
            const ssize_t sent = ::send(fd, bytes.data() + offset, bytes.size() - offset, MSG_DONTWAIT | MSG_NOSIGNAL);

            if (sent > 0) {
                offset += static_cast<std::size_t>(sent);
                continue;
            }

            if (sent == 0) {
                throw std::runtime_error("send() failed");
            }

            if (errno == EINTR) {
                continue;
            }


            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Deliberately busy-spin until send-buffer space exists.
            }

            throw std::system_error(
                errno,
                std::generic_category(),
                "send failed");
        }
    }


    void Socket::Shutdown(int how) {
        const int result = ::shutdown(fd, static_cast<int>(how));
        if (result == -1) { throw std::runtime_error("shutdown() failed"); }
    }

    bool Socket::IsValid() const noexcept {
        return fd != -1;
    }
    Socket::Socket(int existing) noexcept {
        fd = existing;
    }
}
