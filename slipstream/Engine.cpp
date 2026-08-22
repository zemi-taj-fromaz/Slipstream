//
// Created by babodev on 16.08.2026..
//

#include "Engine.h"

#include <cstring>
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif
#include <stdexcept>

namespace {

inline void CpuRelax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
#error "CpuRelax is not implemented for this architecture"
#endif
}

} // namespace

Engine::Engine(const SlipstreamConfig& slipstream,
               rigtorp::SPSCQueue<MarketEvent>& in,
               rigtorp::SPSCQueue<slipstream::codec::OrderEntryClientMessage>& out)
    : trade_manager(slipstream),
      ingress(in),
      egress(out) {}

void Engine::Run() {
    while (running.load(std::memory_order_relaxed)) {
        MarketEvent* event = ingress.front();
        if (event == nullptr) {
            CpuRelax();
            continue;
        }

        const TradeResult result = trade_manager.Push(*event);

        if (result == TradeResult::UserTradeAccepted ||
            result == TradeResult::UserTradeRejected) {
            const auto* trade = std::get_if<Trade>(&event->payload);
            if (trade == nullptr) {
                throw std::logic_error(
                    "user trade result produced for a non-trade event");
            }

            slipstream::codec::NewOrderMessage order{
                .client_order_id = next_client_order_id++,
                .status = result == TradeResult::UserTradeAccepted
                    ? slipstream::codec::NewOrderStatus::accepted
                    : slipstream::codec::NewOrderStatus::rejected,
                .ts_ns = event->ts,
                .trade_id = trade->id,

                .side = trade->aggressor == 'S'
                    ? slipstream::codec::OrderSide::sell
                    : slipstream::codec::OrderSide::buy,
                .qty = trade->qty,
                .limit_px = trade->price,
            };
            std::memcpy(order.symbol, event->symbol, sizeof(order.symbol));

            slipstream::codec::OrderEntryClientMessage outbound{order};
            while (!egress.try_push(outbound)) {
                CpuRelax();
            }
        }
        ingress.pop();
    }
}

void Engine::Stop() noexcept {
    running.store(false, std::memory_order_relaxed);
}
