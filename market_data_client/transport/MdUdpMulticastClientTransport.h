#ifndef SLIPSTREAM_MD_UDP_MULTICAST_CLIENT_TRANSPORT_H
#define SLIPSTREAM_MD_UDP_MULTICAST_CLIENT_TRANSPORT_H

#include "client_transport.h"
#include "socket.h"

#include <cstdint>
#include <string>

class MdUdpMulticastClientTransport final : public IClientTransport {
public:
    MdUdpMulticastClientTransport(
        std::string feed_a_group,
        std::uint16_t feed_a_port,
        std::string feed_b_group,
        std::uint16_t feed_b_port,
        std::string multicast_interface = "0.0.0.0");

    utils::ConnectionResult Send(const MarketEvent& event) override;

private:
    std::string feed_a_group_;
    std::uint16_t feed_a_port_{};
    std::string feed_b_group_;
    std::uint16_t feed_b_port_{};
    std::uint64_t sequence_{1};
    utils::UdpSocket feed_a_;
    utils::UdpSocket feed_b_;
};

#endif
