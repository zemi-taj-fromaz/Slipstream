//
// Created by babodev on 12.08.2026..
//

#ifndef SLIPSTREAM_MARKET_EVENT_H
#define SLIPSTREAM_MARKET_EVENT_H
#include <variant>
#include <cstdint>

struct Quote {
    std::int64_t bid_price;
    std::uint32_t bid_qty;
    std::int64_t ask_price;
    std::uint32_t ask_qty;
};

struct Trade {
    std::int64_t price;
    std::int64_t id;
    std::uint32_t qty;
    char aggressor{'?'};
};

struct MarketEvent {
    std::uint64_t ts;
    char symbol[12];
    std::variant<Quote, Trade> payload;

};

#endif //SLIPSTREAM_MARKET_EVENT_H
