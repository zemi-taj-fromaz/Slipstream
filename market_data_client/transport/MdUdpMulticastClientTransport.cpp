#include "MdUdpMulticastClientTransport.h"

#include "slipstream_codec/market_data_codec.h"

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>

MdUdpMulticastClientTransport::MdUdpMulticastClientTransport(
    std::string feed_a_group,
    std::uint16_t feed_a_port,
    std::string feed_b_group,
    std::uint16_t feed_b_port,
    std::string multicast_interface)
    : feed_a_group_{std::move(feed_a_group)},
      feed_a_port_{feed_a_port},
      feed_b_group_{std::move(feed_b_group)},
      feed_b_port_{feed_b_port} {
    constexpr int send_buffer_size = 1024 * 1024;
    constexpr std::uint8_t multicast_ttl = 1;

    feed_a_.SetSendBufferSize(send_buffer_size);
    feed_a_.SetMulticastTtl(multicast_ttl);
    feed_a_.SetMulticastLoop();
    feed_a_.SetMulticastInterface(multicast_interface.c_str());

    feed_b_.SetSendBufferSize(send_buffer_size);
    feed_b_.SetMulticastTtl(multicast_ttl);
    feed_b_.SetMulticastLoop();
    feed_b_.SetMulticastInterface(multicast_interface.c_str());
}

utils::ConnectionResult MdUdpMulticastClientTransport::Send(
    const MarketEvent& event) {
    if (!std::holds_alternative<Quote>(event.payload)) {
        throw std::invalid_argument(
            "MD UDP multicast transport requires a quote");
    }

    std::array<
        std::byte,
        slipstream::codec::max_multicast_datagram_size> buffer{};
    const std::size_t encoded_size =
        slipstream::codec::EncodeMulticastMarketData(
            sequence_,
            event,
            buffer);
    const std::span<const std::byte> datagram{
        buffer.data(),
        encoded_size};

    feed_a_.SendDatagram(
        datagram,
        feed_a_group_.c_str(),
        feed_a_port_);
    feed_b_.SendDatagram(
        datagram,
        feed_b_group_.c_str(),
        feed_b_port_);

    ++sequence_;
    return utils::ConnectionResult::Complete;
}
