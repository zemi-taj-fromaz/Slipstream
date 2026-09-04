#ifndef SLIPSTREAM_ISERVERTRANSPORT_H
#define SLIPSTREAM_ISERVERTRANSPORT_H

#include "ExecutionReport.h"

namespace slipstream {

class IServerTransport {
public:
    virtual ~IServerTransport() = default;

    virtual void Run() = 0;
    virtual void Stop() = 0;
    virtual void NotifyOutboundReady() = 0;

    [[nodiscard]]
    virtual TickToOrderStatistics
    GetTickToOrderStatistics() const = 0;

    [[nodiscard]]
    virtual const TickToOrderHistogram&
    GetTickToOrderHistogram() const noexcept = 0;
};

}

#endif
