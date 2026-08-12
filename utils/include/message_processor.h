#ifndef SLIPSTREAM_MESSAGE_PROCESSOR_H
#define SLIPSTREAM_MESSAGE_PROCESSOR_H

#include "market_event.h"

#include <vector>

class IProcessMsgClass {
public:
    virtual ~IProcessMsgClass() = default;

    virtual void Sink(const MarketEvent& event);
};

void ProcessRowsByTimestamp(
    const std::vector<MarketEvent>& rows,
    IProcessMsgClass& processor);

#endif // SLIPSTREAM_MESSAGE_PROCESSOR_H
