//
// Created by babodev on 18.08.2026..
//

#ifndef SLIPSTREAM_TRADEMANAGER_H
#define SLIPSTREAM_TRADEMANAGER_H

#include "VwapWindow.h"
#include <cstdint>
#include <memory>
#include <variant>
#include "market_event.h"

struct L1Book {
    std::int64_t bid_price{};
    std::uint32_t bid_qty{};

    std::int64_t ask_price{};
    std::uint32_t ask_qty{};
};

struct MarketUpdateResult {
    __int128_t market_pq_delta{};
    std::uint64_t market_qty_delta{};
};

struct UserTradeResult {
    TradeDecision decision{};
    std::int64_t rolling_vwap{};
};

using TradeManagerResult = std::variant<
    MarketUpdateResult,
    UserTradeResult>;

class TradeManager {
public:
    TradeManager(const SlipstreamConfig& slipstream);

    TradeManagerResult Push(MarketEvent& event);

private:
    VwapWindow vwap_window;
    std::unique_ptr<L1Book> book;
};

#endif //SLIPSTREAM_TRADEMANAGER_H
