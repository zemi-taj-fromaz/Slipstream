#include "md_msg_controller.h"

#include "slipstream_codec/market_data_codec.h"

#include <array>
#include <cstddef>

MDMsgController::MDMsgController(const char* host, std::uint16_t port) {
    constexpr int send_buffer_size = 1024 * 1024;
    socket_.SetTcpNoDelay();
    socket_.SetSendBufferSize(send_buffer_size);
    socket_.Connect(host, port);
}

void MDMsgController::Send(const MarketEvent& event) {
    std::array<std::byte, slipstream::codec::max_market_data_frame_size> buffer{};
    const std::size_t encoded_size =
        slipstream::codec::EncodeMarketEvent(event, buffer);

    socket_.SendAll({buffer.data(), encoded_size});
}
