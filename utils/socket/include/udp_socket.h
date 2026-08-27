//
// Created by Adminstudio on 8/27/2026.
//
#ifndef SLIPSTREAM_UDP_SOSCKET_H
#define SLIPSTREAM_UDP_SOSCKET_H

#include <cstdint>
#include <string_view>
#include <span>
#include <sys/types.h>

namespace utils {
enum class ConnectionResult : std::uint8_t {
  Complete,
  PeerDisconnected
};

class UdpSocket {
public:
  UdpSocket();
  ~UdpSocket();

  UdpSocket(const Socket&) = delete;
  UdpSocket& operator=(const Socket&) = delete;

  UdpSocket(Socket&&) noexcept;
  UdpSocket& operator=(Socket&&) noexcept;

  void SetReuseAddress(bool enabled = true);
  void SetReceiveBufferSize(int size);
  void SetSendBufferSize(int size);
  void SetTcpNoDelay(bool enabled = true);
  void Bind(std::uint16_t port);
  void Bind(const char* address, std::uint16_t port);
  void Listen(int backlog = 8); // waits for connection
  [[nodiscard]]

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


#endif // SLIPSTREAM_UDP_SOSCKET_H
