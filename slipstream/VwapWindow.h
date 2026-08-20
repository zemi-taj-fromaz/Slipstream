//
// Created by babodev on 20.08.2026..
//

#ifndef SLIPSTREAM_VWAPWINDOW_H
#define SLIPSTREAM_VWAPWINDOW_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "SlipstreamConfig.h"

inline constexpr std::size_t vwap_capacity = 256u;

enum class TradeOrigin : std::uint16_t {
    Market,
    User
};

struct TradePrint {
    __int128_t pq{};
    std::uint64_t ts_ns{};
    std::int64_t price{};
    std::uint32_t qty{};
    TradeOrigin origin{};
};

class VwapWindow {
public:
    explicit VwapWindow(const SlipstreamConfig& slipstream);

    void push(TradePrint trade_print);

private:
    void evictExpired(std::uint64_t now_ns);
    void insert(const TradePrint& trade_print);
    void recomputeMetrics();

    double participation_cap;
    std::uint64_t vwap_window_ns;
    std::uint32_t max_qty;
    std::uint32_t band_bps;

    std::array<TradePrint, vwap_capacity> trades{};

    std::size_t head{};
    std::size_t tail{};
    std::size_t count{};

    std::uint64_t vwap_start_ts{};
    std::uint64_t vwap_end_ts{};

    std::uint64_t vwap_first_ts{};
    bool warmup_gate_passed{false};
    std::uint32_t observedQuotes{};

    __int128_t rolling_vwap{};
    __int128_t sum_pq{};
    std::uint64_t sum_market_qty{};
    std::uint64_t sum_user_qty{};
    std::uint64_t sum_qty{};

    double current_participation{};
};

#endif // SLIPSTREAM_VWAPWINDOW_H
