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
    : config_(slipstream),
      trade_manager(slipstream),
      ingress(in),
      egress(out),
      ingress_generation(generation),
      notify_egress(std::move(egress_notifier)) {}

void Engine::Run() {
    while (running.load(std::memory_order_relaxed)) {
        if (MarketEvent* event = ingress.front()) {
            const TradeManagerResult manager_result =
                trade_manager.Push(*event);

            if (const auto* market =
                    std::get_if<MarketUpdateResult>(&manager_result)) {
                execution_report_.market_qty +=
                    market->market_qty_delta;
                execution_report_.market_notional_sum +=
                    market->market_notional_delta;

                ingress.pop();
                continue;
            }

            const auto& user = std::get<UserTradeResult>(manager_result);
            const TradeDecision& decision = user.decision;
            const TradeResult result = decision.result;
            const auto* trade = std::get_if<Trade>(&event->payload);
            if (trade == nullptr) {
                throw std::logic_error(
                    "user trade result produced for a non-trade event");
            }

            execution_report_.submitted_qty += trade->qty;

            if (result == TradeResult::UserTradeAccepted) {
                const __int128_t notional =
                    static_cast<__int128_t>(trade->price) * trade->qty;
                execution_report_.executed_qty += trade->qty;
                execution_report_.executed_notional_sum += notional;

                if (decision.side == TradeSide::Buy) {
                    execution_report_.buy_qty += trade->qty;
                    execution_report_.buy_notional_sum += notional;
                } else if (decision.side == TradeSide::Sell) {
                    execution_report_.sell_qty += trade->qty;
                    execution_report_.sell_notional_sum += notional;
                } else {
                    throw std::logic_error(
                        "accepted user trade has unknown side");
                }
            }

            if (IsUserTradeResult(result)) {
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
                        << " vwap=" << user.rolling_vwap
                        << " band_bps=" << config_.band_bps
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

const ExecutionReport& Engine::GetExecutionReport() const noexcept {
    return execution_report_;
}
