#ifndef SLIPSTREAM_CLIENT_TRANSPORT_H
#define SLIPSTREAM_CLIENT_TRANSPORT_H

#include "market_event.h"
#include "socket.h"

#include <chrono>
#include <thread>

class IClientTransport {
public:
    virtual ~IClientTransport() = default;

    virtual utils::ConnectionResult Send(
        const MarketEvent& event) = 0;

    virtual utils::ConnectionResult ProcessInboundUntil(
        std::chrono::steady_clock::time_point deadline) {
        std::this_thread::sleep_until(deadline);
        return utils::ConnectionResult::Complete;
    }
};

#endif
