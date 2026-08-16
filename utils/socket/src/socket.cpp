//
// Created by babodev on 15.08.2026..
//
#include "socket.h"
#include <string_view>
#include <utility>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
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


    void Socket::Bind(std::uint16_t port)
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        const int result  = ::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        if (result == -1) { throw std::runtime_error("bind() failed"); }
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

    ssize_t Socket::Send(std::span<const std::byte> buffer) {
        return ::send(fd, buffer.data(), buffer.size(), MSG_DONTWAIT);
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
