//
// Created by babodev on 20.08.2026..
//

#include "VwapWindow.h"

#include <cmath>
#include <stdexcept>
#include <algorithm>

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
           result == TradeResult::UserTradePartial_MaxQuantity ||
           result == TradeResult::UserTradePartial_ParticipationCap ||
           IsUserTradeRejected(result);
}

bool IsUserTradeRejected(const TradeResult result) noexcept {
    return result == TradeResult::UserTradeRejectedBand ||
           result == TradeResult::UserTradeRejected_ParticipationCap;
}

const char* TradeRejectionReason(const TradeResult result) noexcept {
    switch (result) {
    case TradeResult::UserTradeRejectedBand:
        return "band_bps";
    case TradeResult::UserTradeRejected_ParticipationCap:
        return "participation_cap";
    case TradeResult::NoOrder:
    case TradeResult::MarketTradeRecorded:
    case TradeResult::UserTradeAccepted:
    case TradeResult::UserTradePartial_MaxQuantity:
    case TradeResult::UserTradePartial_ParticipationCap:
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

    const std::uint32_t submitted_qty{trade_print.qty};

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
            submitted_qty,
            0,
        };
    }

    TradeResult tradeResult_{TradeResult::UserTradeAccepted};

    if (trade_print.qty > max_qty) {
        trade_print.qty = max_qty;
        tradeResult_ = TradeResult::UserTradePartial_MaxQuantity;
    }

    const auto market_quantity_after_trade = sum_market_qty;
    const auto user_quantity_after_trade =
        sum_user_qty + trade_print.qty;
    const auto total_quantity_after_trade =
        market_quantity_after_trade + user_quantity_after_trade;

    const auto participation_after_trade = total_quantity_after_trade == 0
        ? 1.0
        : static_cast<double>(user_quantity_after_trade) /
          static_cast<double>(total_quantity_after_trade);

    if (participation_after_trade  > participation_cap) {
        const auto max_user_qty_at_cap =
            participation_cap * static_cast<double>(market_quantity_after_trade) /
            (1.0 - participation_cap);

        const auto allowed_qty = max_user_qty_at_cap <=
                static_cast<double>(sum_user_qty)
            ? 0U
            : static_cast<std::uint32_t>(
                max_user_qty_at_cap - static_cast<double>(sum_user_qty));

        if (allowed_qty == 0) {
            return {
                TradeResult::UserTradeRejected_ParticipationCap,
                trade_print.side,
                submitted_qty,
                0,
            };
        }

        trade_print.qty = std::min(trade_print.qty, allowed_qty);
        tradeResult_ = TradeResult::UserTradePartial_ParticipationCap;
    }

    return {
        tradeResult_,
        trade_print.side,
        submitted_qty,
        trade_print.qty
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
