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

enum class TradeSide : std::uint8_t {
    Unknown,
    Buy,
    Sell,
};

struct TradePrint {
    __int128_t pq{};
    std::uint64_t ts_ns{};
    std::int64_t price{};
    std::uint32_t qty{};
    TradeOrigin origin{};
    TradeSide side{TradeSide::Unknown};
};

enum class TradeResult : std::uint8_t {
    NoOrder = 0,
    MarketTradeRecorded = 1,

    UserTradeAccepted = 2,

    UserTradeRejectedBand = 4,
    UserTradeRejected_ParticipationCap = 8,
    UserTradePartial_MaxQuantity = 16,
    UserTradePartial_ParticipationCap = 32,
};

struct TradeDecision {
    TradeResult result{TradeResult::NoOrder};
    TradeSide side{TradeSide::Unknown};
    std::uint32_t submitted_qty{};
    std::uint32_t executed_qty{};
};

[[nodiscard]] bool IsUserTradeResult(TradeResult result) noexcept;
[[nodiscard]] bool IsUserTradeRejected(TradeResult result) noexcept;
[[nodiscard]] const char* TradeRejectionReason(TradeResult result) noexcept;

class VwapWindow {
public:
    explicit VwapWindow(const SlipstreamConfig& slipstream);

    TradeDecision push(TradePrint trade_print);
    [[nodiscard]] std::int64_t RollingVwap() const noexcept;

private:
    [[nodiscard]] TradeDecision checkConstraints(TradePrint& trade_print);
    void resolveSide(TradePrint& trade_print) const;
    void evictExpired(std::uint64_t now_ns);
    void insert(const TradePrint& trade_print);
    void recomputeMetrics();

    double participation_cap;
    std::uint64_t vwap_window_ns;
    std::uint32_t max_qty;
    std::uint64_t band_bps_units;

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
    __int128_t sum_market_pq{};
    std::uint64_t sum_market_qty{};
    std::uint64_t sum_user_qty{};
};

#endif // SLIPSTREAM_VWAPWINDOW_H
