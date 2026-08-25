//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_ENGINE_H
#define SLIPSTREAM_ENGINE_H

#include "SlipstreamConfig.h"
#include "ExecutionReport.h"
#include "TradeManager.h"
#include "slipstream_codec/market_data_codec.h"
#include "rigtorp/SPSCQueue.h"
#include "market_event.h"

#include <atomic>
#include <cstdint>
#include <functional>

class Engine {
    public:
    Engine(const SlipstreamConfig& slipstream,
        rigtorp::SPSCQueue<MarketEvent>& in,
        rigtorp::SPSCQueue<slipstream::codec::OrderEntryClientMessage>& out,
        std::atomic<std::uint64_t>& ingress_generation,
        std::function<void()> notify_egress);

        void Run();
        void Stop() noexcept;
        [[nodiscard]]
        const ExecutionReport& GetExecutionReport() const noexcept;


    private:
        [[nodiscard]]
        bool PushOutbound(
            slipstream::codec::OrderEntryClientMessage outbound);
        const SlipstreamConfig& config_;
        TradeManager trade_manager;
        ExecutionReport execution_report_;

        rigtorp::SPSCQueue<MarketEvent>& ingress;
        rigtorp::SPSCQueue<slipstream::codec::OrderEntryClientMessage>& egress;
        std::atomic<std::uint64_t>& ingress_generation;
        std::function<void()> notify_egress;

        std::atomic_bool running{true};
        std::uint64_t next_client_order_id{1};

};


#endif //SLIPSTREAM_ENGINE_H
