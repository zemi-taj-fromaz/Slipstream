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
#include <utility>

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

slipstream::codec::RejectReason ToRejectReason(
    const TradeResult result) noexcept {
    if (result == TradeResult::UserTradeRejectedBand) {
        return slipstream::codec::RejectReason::price;
    }
    if (result == TradeResult::UserTradeRejected_ParticipationCap ||
        result == TradeResult::UserTradePartial_ParticipationCap) {
        return slipstream::codec::RejectReason::risk;
    }
    if (result == TradeResult::UserTradePartial_MaxQuantity) {
        return slipstream::codec::RejectReason::size;
    }

    return slipstream::codec::RejectReason::none;
}

} // namespace

Engine::Engine(const SlipstreamConfig& slipstream,
               slipstream::MarketEventQueue& in,
               slipstream::OrderEntryQueue& out,
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
        MarketEvent event{};
        if (!ingress.pop(event)) {
            const std::uint64_t observed = ingress_generation.load(
                std::memory_order_acquire);

            if (!ingress.pop(event)) {
                if (running.load(std::memory_order_acquire)) {
                    ingress_generation.wait(
                        observed,
                        std::memory_order_acquire);
                }
                continue;
            }
        }

        const TradeManagerResult manager_result = trade_manager.Push(event);

        if (const auto* market =
                std::get_if<MarketUpdateResult>(&manager_result)) {
            execution_report_.market_qty +=
                market->market_qty_delta;
            execution_report_.market_pq_sum +=
                market->market_pq_delta;
            continue;
        }

        const auto& user = std::get<UserTradeResult>(manager_result);
        const TradeDecision& decision = user.decision;
        const TradeResult result = decision.result;
        const auto* trade = std::get_if<Trade>(&event.payload);
        if (trade == nullptr) {
            throw std::logic_error(
                "user trade result produced for a non-trade event");
        }

        execution_report_.submitted_qty += decision.submitted_qty;

        const bool rejected = IsUserTradeRejected(result);
        const bool partial =
            result == TradeResult::UserTradePartial_MaxQuantity ||
            result == TradeResult::UserTradePartial_ParticipationCap;
        const bool executed = !rejected && decision.executed_qty != 0;

        if (executed) {
            const __int128_t pq =
                static_cast<__int128_t>(trade->price) * decision.executed_qty;
            execution_report_.executed_qty += decision.executed_qty;
            execution_report_.executed_pq_sum += pq;

            if (decision.side == TradeSide::Buy) {
                execution_report_.buy_qty += decision.executed_qty;
                execution_report_.buy_pq_sum += pq;
            } else if (decision.side == TradeSide::Sell) {
                execution_report_.sell_qty += decision.executed_qty;
                execution_report_.sell_pq_sum += pq;
            } else {
                throw std::logic_error(
                    "executed user trade has unknown side");
            }
        }

        if (IsUserTradeResult(result)) {
            if (decision.side == TradeSide::Unknown) {
                throw std::logic_error(
                    "user trade decision has unknown side");
            }

            if (rejected) {
                std::cout
                    << "[slipstream] Rejecting NewOrder reason="
                    << TradeRejectionReason(result)
                    << " trade_id=" << trade->id
                    << " symbol=" << SymbolText(
                        event.symbol,
                        sizeof(event.symbol))
                    << " qty=" << trade->qty
                    << " price=" << trade->price
                    << " vwap=" << user.rolling_vwap
                    << " band_bps=" << config_.band_bps
                    << '\n'
                    << std::flush;
            }

            const std::uint64_t client_order_id = next_client_order_id++;

            slipstream::codec::NewOrderMessage order{
                .client_order_id = client_order_id,
                .status = rejected
                    ? slipstream::codec::NewOrderStatus::rejected
                    : slipstream::codec::NewOrderStatus::accepted,
                .ts_ns = event.ts,
                .trade_id = trade->id,

                .side = decision.side == TradeSide::Sell
                    ? slipstream::codec::OrderSide::sell
                    : slipstream::codec::OrderSide::buy,
                .qty = decision.submitted_qty,
                .limit_px = trade->price,
            };
            std::memcpy(order.symbol, event.symbol, sizeof(order.symbol));

            if (!PushOutbound(order)) {
                return;
            }

            const slipstream::codec::RejectReason reason =
                ToRejectReason(result);

            if (rejected) {
                const slipstream::codec::ExecReportMessage reject_report{
                    .client_order_id = client_order_id,
                    .ts_ns = event.ts,
                    .status = slipstream::codec::ExecStatus::reject,
                    .filled_qty = 0,
                    .avg_px = 0,
                    .reason_code = reason,
                };

                if (!PushOutbound(reject_report)) {
                    return;
                }
            } else {
                const slipstream::codec::ExecReportMessage ack_report{
                    .client_order_id = client_order_id,
                    .ts_ns = event.ts,
                    .status = slipstream::codec::ExecStatus::ack,
                    .filled_qty = 0,
                    .avg_px = 0,
                    .reason_code = slipstream::codec::RejectReason::none,
                };

                if (!PushOutbound(ack_report)) {
                    return;
                }

                const slipstream::codec::ExecReportMessage final_report{
                    .client_order_id = client_order_id,
                    .ts_ns = event.ts,
                    .status = partial
                        ? slipstream::codec::ExecStatus::partial
                        : slipstream::codec::ExecStatus::fill,
                    .filled_qty = decision.executed_qty,
                    .avg_px = trade->price,
                    .reason_code = reason,
                };

                if (!PushOutbound(final_report)) {
                    return;
                }
            }

            notify_egress();
        }
    }
}

void Engine::Stop() noexcept {
    running.store(false, std::memory_order_release);
    ingress_generation.fetch_add(1, std::memory_order_release);
    ingress_generation.notify_one();
}

bool Engine::PushOutbound(
    slipstream::codec::OrderEntryClientMessage outbound) {
    while (!egress.push(outbound)) {
        if (!running.load(std::memory_order_acquire)) {
            return false;
        }

        CpuRelax();
    }

    return true;
}

const ExecutionReport& Engine::GetExecutionReport() const noexcept {
    return execution_report_;
}
