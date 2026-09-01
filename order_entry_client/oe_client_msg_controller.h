#ifndef SLIPSTREAM_OE_CLIENT_MSG_CONTROLLER_H
#define SLIPSTREAM_OE_CLIENT_MSG_CONTROLLER_H

#include "message_processor.h"
#include "slipstream_codec/market_data_codec.h"
#include "socket.h"

#include <chrono>
#include <cstdint>
#include <poll.h>
#include <string>

#include <spdlog/logger.h>

class OEClientMsgController final : public IMsgController {
public:
    OEClientMsgController(
        const char* host,
        std::uint16_t port,
        spdlog::logger& logger);

    utils::ConnectionResult Send(const MarketEvent& event) override;
    utils::ConnectionResult ProcessInboundUntil(
        std::chrono::steady_clock::time_point deadline) override;

private:
    [[nodiscard]] static std::string Symbol(const char (&symbol)[12]);

    utils::ConnectionResult ReceiveAvailable();
    void HandleMessage(
        const slipstream::codec::OrderEntryClientMessage& message);

    slipstream::codec::SessionState sessionState{slipstream::codec::SessionState::open};

    utils::TcpSocket socket_;
    pollfd poll_descriptor_{};
    slipstream::codec::ClientSideDecoder decoder_;
    spdlog::logger& logger_;
};

#endif // SLIPSTREAM_OE_CLIENT_MSG_CONTROLLER_H
