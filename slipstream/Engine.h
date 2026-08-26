//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_ENGINE_H
#define SLIPSTREAM_ENGINE_H

#include "SlipstreamConfig.h"
#include "Queues.h"
#include "ExecutionReport.h"
#include "TradeManager.h"

#include <atomic>
#include <cstdint>
#include <functional>

class Engine {
    public:
    Engine(const SlipstreamConfig& slipstream,
        slipstream::MarketEventQueue& in,
        slipstream::OrderEntryQueue& out,
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

        slipstream::MarketEventQueue& ingress;
        slipstream::OrderEntryQueue& egress;
        std::atomic<std::uint64_t>& ingress_generation;
        std::function<void()> notify_egress;

        std::atomic_bool running{true};
        std::uint64_t next_client_order_id{1};

};


#endif //SLIPSTREAM_ENGINE_H
