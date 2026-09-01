#ifndef SLIPSTREAM_MD_MSG_CONTROLLER_H
#define SLIPSTREAM_MD_MSG_CONTROLLER_H

#include "message_processor.h"
#include "socket.h"

#include <cstdint>

class MDMsgController final : public IMsgController {
public:
    MDMsgController(const char* host, std::uint16_t port);

    utils::ConnectionResult Send(const MarketEvent& event) override;

private:
    utils::TcpSocket socket_;
};

#endif // SLIPSTREAM_MD_MSG_CONTROLLER_H
