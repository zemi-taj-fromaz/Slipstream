//
// Created by babodev on 20.08.2026..
//

#include "VwapWindow.h"

#include <cmath>
#include <stdexcept>

namespace {
constexpr std::uint64_t nanoseconds_per_millisecond = 1'000'000ULL;
constexpr std::uint64_t band_bps_precision = 1'000ULL;
constexpr std::uint64_t basis_points_per_unit = 10'000ULL;
constexpr std::uint64_t band_denominator =
    basis_points_per_unit * band_bps_precision;

std::uint64_t ScaleBandBps(const double band_bps) {
    if (!std::isfinite(band_bps) ||
        band_bps < 0.0 ||
        band_bps > static_cast<double>(basis_points_per_unit)) {
        throw std::invalid_argument(
            "band_bps must be a finite value in [0, 10000]");
    }

    return static_cast<std::uint64_t>(
        std::llround(band_bps * band_bps_precision));
}
}

bool IsUserTradeResult(const TradeResult result) noexcept {
    return result == TradeResult::UserTradeAccepted ||
           IsUserTradeRejected(result);
}

bool IsUserTradeRejected(const TradeResult result) noexcept {
    return result == TradeResult::UserTradeRejectedBand ||
           result == TradeResult::UserTradeRejectedMaxQuantity ||
           result == TradeResult::UserTradeRejectedParticipationCap;
}

const char* TradeRejectionReason(const TradeResult result) noexcept {
    switch (result) {
    case TradeResult::UserTradeRejectedBand:
        return "band_bps";
    case TradeResult::UserTradeRejectedMaxQuantity:
        return "max_quantity";
    case TradeResult::UserTradeRejectedParticipationCap:
        return "participation_cap";
    case TradeResult::NoOrder:
    case TradeResult::MarketTradeRecorded:
    case TradeResult::UserTradeAccepted:
        return "not_rejected";
    }

    return "unknown";
}

VwapWindow::VwapWindow(const SlipstreamConfig& slipstream)
    : participation_cap(slipstream.participation_cap),
      vwap_window_ns(
          static_cast<std::uint64_t>(slipstream.vwap_window_ms) *
          nanoseconds_per_millisecond),
      max_qty(slipstream.max_quantity),
      band_bps_units(ScaleBandBps(slipstream.band_bps)) {
}

TradeDecision VwapWindow::push(TradePrint trade_print) {
    evictExpired(trade_print.ts_ns);

    const TradeDecision decision = checkConstraints(trade_print);
    if (decision.result == TradeResult::NoOrder ||
        IsUserTradeRejected(decision.result)) {
        return decision;
    }

    insert(trade_print);
    recomputeMetrics();
    return decision;
}

TradeDecision VwapWindow::checkConstraints(TradePrint& trade_print) {
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

        return {
            TradeResult::MarketTradeRecorded,
            trade_print.side,
        };
    }

    if (!warmup_gate_passed || count < 10) {
        return {
            TradeResult::NoOrder,
            trade_print.side,
        };
    }

    resolveSide(trade_print);

    const __int128_t scaled_trade_price =
        static_cast<__int128_t>(trade_print.price) *
        band_denominator;

    bool band_crossed = false;

    switch (trade_print.side) {
    case TradeSide::Buy:
        band_crossed =
            scaled_trade_price <=
            rolling_vwap *
                (band_denominator - band_bps_units);
        break;
    case TradeSide::Sell:
        band_crossed =
            scaled_trade_price >=
            rolling_vwap *
                (band_denominator + band_bps_units);
        break;
    case TradeSide::Unknown:
        throw std::logic_error(
            "user trade side was not resolved");
    }

    if (!band_crossed) {
        return {
            TradeResult::UserTradeRejectedBand,
            trade_print.side,
        };
    }

    if (trade_print.qty > max_qty) {
        return {
            TradeResult::UserTradeRejectedMaxQuantity,
            trade_print.side,
        };
    }

    const auto user_quantity_after_trade =
        sum_user_qty + trade_print.qty;

    const auto participation_after_trade = sum_market_qty == 0
        ? 1.0
        : static_cast<double>(user_quantity_after_trade) /
          static_cast<double>(sum_market_qty);

    return {
        participation_after_trade <= participation_cap
            ? TradeResult::UserTradeAccepted
            : TradeResult::UserTradeRejectedParticipationCap,
        trade_print.side,
    };
}

void VwapWindow::resolveSide(TradePrint& trade_print) const {
    if (trade_print.side != TradeSide::Unknown) {
        return;
    }

    trade_print.side = trade_print.price <= rolling_vwap
        ? TradeSide::Buy
        : TradeSide::Sell;
}

void VwapWindow::evictExpired(const std::uint64_t now_ns) {
    const std::uint64_t cutoff =
        now_ns >= vwap_window_ns
            ? now_ns - vwap_window_ns
            : 0;

    while (count > 0 && trades[tail].ts_ns < cutoff) {
        const TradePrint& expired = trades[tail];

        if (expired.origin == TradeOrigin::Market) {
            sum_market_pq -= expired.pq;
            sum_market_qty -= expired.qty;
        } else {
            sum_user_qty -= expired.qty;
        }

        tail = (tail + 1) & (vwap_capacity - 1);
        --count;
    }

    vwap_start_ts = count > 0 ? trades[tail].ts_ns : 0;
    recomputeMetrics();
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

    if (trade_print.origin == TradeOrigin::Market) {
        sum_market_pq += trade_print.pq;
        sum_market_qty += trade_print.qty;
    } else {
        sum_user_qty += trade_print.qty;
    }
}

void VwapWindow::recomputeMetrics() {
    if (sum_market_qty == 0) {
        rolling_vwap = 0;
        return;
    }

    rolling_vwap = sum_market_pq / sum_market_qty;
}

std::int64_t VwapWindow::RollingVwap() const noexcept {
    return static_cast<std::int64_t>(rolling_vwap);
}
