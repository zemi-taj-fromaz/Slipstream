//
// Created by babodev on 15.08.2026..
//

#ifndef SLIPSTREAM_SOCKET_H
#define SLIPSTREAM_SOCKET_H
#include <cstdint>
#include <string_view>
#include <span>
#include <sys/types.h>


namespace utils {
    class Socket {
    public:
        Socket();
        ~Socket();

        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        Socket(Socket&&) noexcept;
        Socket& operator=(Socket&&) noexcept;

        void SetReuseAddress(bool enabled = true);
        void SetReceiveBufferSize(int size);
        void SetSendBufferSize(int size);
        void SetTcpNoDelay(bool enabled = true);
        void Bind(std::uint16_t port);
        void Listen(int backlog = 8); // waits for connection
        [[nodiscard]]
        Socket Accept();

        void Connect(const char* address, std::uint16_t port);

        ssize_t Recv(std::span<std::byte> buffer);
        void SendAll(std::span<const std::byte> bytes);
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
}

#endif //SLIPSTREAM_SOCKET_H
