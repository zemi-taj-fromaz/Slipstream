//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_ENGINE_H
#define SLIPSTREAM_ENGINE_H

#include "SlipstreamConfig.h"
#include "TradeManager.h"
#include "rigtorp/SPSCQueue.h"

struct L1Book {
    std::int64_t bid_price{};
    std::uint32_t bid_qty{};

    std::int64_t ask_price{};
    std::uint32_t ask_qty{};
};

class Engine {
    public:
    Engine(const SlipstreamConfig& slipstream,
        rigtorp::SPSCQueue<MarketEvent>& in,
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out);


    private:
        TradeManager trade_manager;
        L1Book book;

    rigtorp::SPSCQueue<MarketEvent>& ingress;
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& egress;

};


#endif //SLIPSTREAM_ENGINE_H
