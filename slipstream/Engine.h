//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_ENGINE_H
#define SLIPSTREAM_ENGINE_H

#include "SlipstreamConfig.h"
#include "TradeManager.h"
#include "rigtorp/SPSCQueue.h"
#include "market_event.h"


class Engine {
    public:
    Engine(const SlipstreamConfig& slipstream,
        rigtorp::SPSCQueue<MarketEvent>& in,
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out);

        void Run();


    private:
        TradeManager trade_manager;

    rigtorp::SPSCQueue<MarketEvent>& ingress;
    rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& egress;

};


#endif //SLIPSTREAM_ENGINE_H
