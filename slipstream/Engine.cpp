//
// Created by babodev on 16.08.2026..
//

#include "Engine.h"

#include <algorithm>
#include <cstring>
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif
#include <iostream>
#include <stdexcept>
#include <string>

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

std::string SymbolText(const char* symbol, const std::size_t size) {
    const char* end = std::find(symbol, symbol + size, '\0');
    return {symbol, end};
}

} // namespace

Engine::Engine(const SlipstreamConfig& slipstream,
               rigtorp::SPSCQueue<MarketEvent>& in,
               rigtorp::SPSCQueue<slipstream::codec::OrderEntryClientMessage>& out,
               std::atomic<std::uint64_t>& generation,
               std::function<void()> egress_notifier)
    : trade_manager(slipstream),
      ingress(in),
      egress(out),
      ingress_generation(generation),
      notify_egress(std::move(egress_notifier)) {}

void Engine::Run() {
    while (running.load(std::memory_order_relaxed)) {
        if (MarketEvent* event = ingress.front()) {
            const TradeDecision decision = trade_manager.Push(*event);
            const TradeResult result = decision.result;

            if (IsUserTradeResult(result)) {
                const auto* trade = std::get_if<Trade>(&event->payload);
                if (trade == nullptr) {
                    throw std::logic_error(
                        "user trade result produced for a non-trade event");
                }
                if (decision.side == TradeSide::Unknown) {
                    throw std::logic_error(
                        "user trade decision has unknown side");
                }

                if (IsUserTradeRejected(result)) {
                    std::cout
                        << "[slipstream] Rejecting NewOrder reason="
                        << TradeRejectionReason(result)
                        << " trade_id=" << trade->id
                        << " symbol=" << SymbolText(
                            event->symbol,
                            sizeof(event->symbol))
                        << " qty=" << trade->qty
                        << " price=" << trade->price
                        << " vwap=" << decision.vwap
                        << " band_bps=" << decision.band_bps
                        << '\n'
                        << std::flush;
                }

                slipstream::codec::NewOrderMessage order{
                    .client_order_id = next_client_order_id++,
                    .status = result == TradeResult::UserTradeAccepted
                        ? slipstream::codec::NewOrderStatus::accepted
                        : slipstream::codec::NewOrderStatus::rejected,
                    .ts_ns = event->ts,
                    .trade_id = trade->id,

                    .side = decision.side == TradeSide::Sell
                        ? slipstream::codec::OrderSide::sell
                        : slipstream::codec::OrderSide::buy,
                    .qty = trade->qty,
                    .limit_px = trade->price,
                };
                std::memcpy(order.symbol, event->symbol, sizeof(order.symbol));

                slipstream::codec::OrderEntryClientMessage outbound{order};
                while (!egress.try_push(outbound)) {
                    if (!running.load(std::memory_order_acquire)) {
                        return;
                    }

                    CpuRelax();
                }

                notify_egress();
            }

            ingress.pop();
            continue;
        }

        const std::uint64_t observed = ingress_generation.load(
            std::memory_order_acquire);

        if (ingress.front() == nullptr &&
            running.load(std::memory_order_acquire)) {
            ingress_generation.wait(
                observed,
                std::memory_order_acquire);
        }
    }
}

void Engine::Stop() noexcept {
    running.store(false, std::memory_order_release);
    ingress_generation.fetch_add(1, std::memory_order_release);
    ingress_generation.notify_one();
}
