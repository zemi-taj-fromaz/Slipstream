//
// Created by babodev on 18.08.2026..
//

#ifndef SLIPSTREAM_TRADEMANAGER_H
#define SLIPSTREAM_TRADEMANAGER_H

#include "VwapWindow.h"
#include <memory>
#include "market_event.h"

struct L1Book {
    std::int64_t bid_price{};
    std::uint32_t bid_qty{};

    std::int64_t ask_price{};
    std::uint32_t ask_qty{};
};

class TradeManager {
public:
    TradeManager(const SlipstreamConfig& slipstream);

    TradeDecision Push(MarketEvent& event);

private:
    VwapWindow vwap_window;
    std::unique_ptr<L1Book> book;
};

#endif //SLIPSTREAM_TRADEMANAGER_H
