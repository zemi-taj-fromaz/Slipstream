//
// Created by babodev on 18.08.2026..
//

#ifndef SLIPSTREAM_TRADEMANAGER_H
#define SLIPSTREAM_TRADEMANAGER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include "SlipstreamConfig.h"
#include "market_event.h"

constexpr std::uint16_t capacity = 256u;


struct L1Book {
    std::int64_t bid_price{};
    std::uint32_t bid_qty{};

    std::int64_t ask_price{};
    std::uint32_t ask_qty{};
};

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

struct VwapWindow {
    std::array<TradePrint ,capacity> Trades {};

    __int128_t rolling_vwap{};

    std::size_t head{};
    std::size_t tail{};
    std::size_t count{};

    std::uint64_t vwap_window_ts{};
    std::uint64_t vwap_start_ts{};
    std::uint64_t vwap_end_ts{};
    std::uint64_t vwap_first_ts{};

    bool warmup_gate_passed{false};

    __int128_t sum_pq{};
    uint32_t sum_qty{};
    uint32_t sum_market_qty{};
    uint32_t sum_my_qty{};

    double current_participation{};

    void push(TradePrint tp) {
        if (tp.origin == TradeOrigin::Market) {

            // ADVANCE HEAD

            if (!count) vwap_start_ts = tp.ts_ns;
            vwap_end_ts = tp.ts_ns;

            Trades[head] = tp;
            head = (head | 1) & (capacity - 1);
            count++;

            sum_pq += tp.pq;
            sum_qty += tp.qty;
            sum_market_qty += tp.qty;

            //ADVANCE TAIL
            while (vwap_start_ts < vwap_end_ts - vwap_window_ts) {

                sum_pq -= Trades[tail].pq;
                sum_qty -= Trades[tail].qty;
                sum_market_qty -= Trades[tail].qty;

                tail = (tail | 1) & (capacity - 1);
                count--;
                vwap_start_ts = Trades[tail].ts_ns;
            }

            //Compute new VWAP and new participation
            current_participation = sum_my_qty / sum_qty;
            rolling_vwap = sum_pq / sum_qty;
        }
        else if (tp.origin == TradeOrigin::User) {
            if (count < 10) {
                vwap_start_ts = tp.ts_ns;
                
            }

        }
    }
private:

};

class TradeManager {
public:

    TradeManager(const SlipstreamConfig& slipstream);

    bool Push(MarketEvent& event);

private:
    std::uint32_t max_qty;
    double participation_cap;
    std::uint32_t band_bps;

    VwapWindow vwap_window;

    std::unique_ptr<L1Book> book;
};

#endif //SLIPSTREAM_TRADEMANAGER_H
