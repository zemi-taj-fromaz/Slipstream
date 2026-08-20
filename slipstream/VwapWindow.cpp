//
// Created by babodev on 20.08.2026..
//

#include "VwapWindow.h"

#include <stdexcept>

namespace {
constexpr std::uint64_t nanoseconds_per_millisecond = 1'000'000ULL;
}

VwapWindow::VwapWindow(const SlipstreamConfig& slipstream)
    : participation_cap(slipstream.participation_cap),
      vwap_window_ns(
          static_cast<std::uint64_t>(slipstream.vwap_window_ms) *
          nanoseconds_per_millisecond),
      max_qty(slipstream.max_quantity),
      band_bps(slipstream.band_bps) {
}

void VwapWindow::push(TradePrint trade_print) {
    evictExpired(trade_print.ts_ns);

    if (trade_print.origin == TradeOrigin::Market) {
        if (!warmup_gate_passed) {
            if (observedQuotes == 0) {
                vwap_first_ts = trade_print.ts_ns;
            }

            ++observedQuotes;

            const bool full_window_elapsed =
                trade_print.ts_ns >= vwap_first_ts &&
                trade_print.ts_ns - vwap_first_ts >= vwap_window_ns;

            if (full_window_elapsed && observedQuotes >= 10) {
                warmup_gate_passed = true;
            }
        }
    } else {
        if (!warmup_gate_passed || trade_print.qty > max_qty || count < 10) {
            return;
        }

        const auto quantity_after_trade = sum_qty + trade_print.qty;
        const auto user_quantity_after_trade =
            sum_user_qty + trade_print.qty;

        const auto participation_after_trade =
            static_cast<double>(user_quantity_after_trade) /
            static_cast<double>(quantity_after_trade);

        if (participation_after_trade > participation_cap) {
            return;
        }
    }

    insert(trade_print);
    recomputeMetrics();
}

void VwapWindow::evictExpired(const std::uint64_t now_ns) {
    const std::uint64_t cutoff =
        now_ns >= vwap_window_ns
            ? now_ns - vwap_window_ns
            : 0;

    while (count > 0 && trades[tail].ts_ns < cutoff) {
        const TradePrint& expired = trades[tail];

        sum_pq -= expired.pq;
        sum_qty -= expired.qty;

        if (expired.origin == TradeOrigin::Market) {
            sum_market_qty -= expired.qty;
        } else {
            sum_user_qty -= expired.qty;
        }

        tail = (tail + 1) & (vwap_capacity - 1);
        --count;
    }

    vwap_start_ts = count > 0 ? trades[tail].ts_ns : 0;
}

void VwapWindow::insert(const TradePrint& trade_print) {
    if (count == vwap_capacity) {
        throw std::runtime_error("VWAP window buffer overflow");
    }

    trades[head] = trade_print;
    head = (head + 1) & (vwap_capacity - 1);
    ++count;

    vwap_end_ts = trade_print.ts_ns;
    if (count == 1) {
        vwap_start_ts = trade_print.ts_ns;
    }

    sum_pq += trade_print.pq;
    sum_qty += trade_print.qty;

    if (trade_print.origin == TradeOrigin::Market) {
        sum_market_qty += trade_print.qty;
    } else {
        sum_user_qty += trade_print.qty;
    }
}

void VwapWindow::recomputeMetrics() {
    if (sum_qty == 0) {
        rolling_vwap = 0;
        current_participation = 0.0;
        return;
    }

    rolling_vwap = sum_pq / sum_qty;
    current_participation =
        static_cast<double>(sum_user_qty) /
        static_cast<double>(sum_qty);
}
