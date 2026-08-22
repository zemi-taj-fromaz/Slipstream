//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_ENGINE_H
#define SLIPSTREAM_ENGINE_H

#include "SlipstreamConfig.h"
#include "TradeManager.h"
#include "slipstream_codec/market_data_codec.h"
#include "rigtorp/SPSCQueue.h"
#include "market_event.h"

#include <atomic>
#include <cstdint>

class Engine {
    public:
    Engine(const SlipstreamConfig& slipstream,
        rigtorp::SPSCQueue<MarketEvent>& in,
        rigtorp::SPSCQueue<slipstream::codec::OrderEntryClientMessage>& out);

        void Run();
        void Stop() noexcept;


    private:
        TradeManager trade_manager;

        rigtorp::SPSCQueue<MarketEvent>& ingress;
        rigtorp::SPSCQueue<slipstream::codec::OrderEntryClientMessage>& egress;

        std::atomic_bool running{true};
        std::uint64_t next_client_order_id{1};

};


#endif //SLIPSTREAM_ENGINE_H
