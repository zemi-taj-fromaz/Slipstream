#ifndef SLIPSTREAM_MD_TCP_CLIENT_TRANSPORT_H
#define SLIPSTREAM_MD_TCP_CLIENT_TRANSPORT_H

#include "client_transport.h"
#include "socket.h"

#include <cstdint>

class MdTcpClientTransport final : public IClientTransport {
public:
    MdTcpClientTransport(const char* host, std::uint16_t port);

    utils::ConnectionResult Send(const MarketEvent& event) override;

private:
    utils::TcpSocket socket_;
};

#endif
