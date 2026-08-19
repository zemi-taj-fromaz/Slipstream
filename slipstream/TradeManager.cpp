//
// Created by babodev on 18.08.2026..
//

#include "TradeManager.h"

#include <array>
#include <stdexcept>

TradeManager::TradeManager(const SlipstreamConfig& slipstream) : max_qty(slipstream.max_quantity), participation_cap(slipstream.participation_cap),band_bps(slipstream.band_bps)
{
    vwap_window.vwap_window_ms = slipstream.vwap_window_ms;
}

bool TradeManager::Push(MarketEvent& event) {
    if (auto* quote = std::get_if<Quote>(&event.payload)) {
        if (!book) {
            book = std::make_unique<L1Book>(L1Book{
                .bid_price = quote->bid_price,
                .bid_qty = quote->bid_qty,
                .ask_price = quote->ask_price,
                .ask_qty = quote->ask_qty,
            });
            return false;
        }

        std::array<TradePrint, 2> inferred_trades{};
        std::size_t inferred_count = 0;

        const auto infer_trade = [&](const std::int64_t price,
                                     const std::uint32_t qty) {
            if (qty == 0) {
                return;
            }

            inferred_trades[inferred_count++] = TradePrint{
                .pq = static_cast<__int128_t>(price) * qty,
                .ts_ns = event.ts,
                .price = price,
                .qty = qty,
                .origin = TradeOrigin::Market,
            };
        };

        if (quote->ask_price > book->ask_price) {
            infer_trade(book->ask_price, book->ask_qty);
        } else if (quote->ask_price == book->ask_price &&
                   quote->ask_qty < book->ask_qty) {
            infer_trade(
                book->ask_price,
                book->ask_qty - quote->ask_qty);
        }

        if (quote->bid_price < book->bid_price) {
            infer_trade(book->bid_price, book->bid_qty);
        } else if (quote->bid_price == book->bid_price &&
                   quote->bid_qty < book->bid_qty) {
            infer_trade(
                book->bid_price,
                book->bid_qty - quote->bid_qty);
        }

        for (std::size_t i = 0; i < inferred_count; ++i) {
            if (vwap_window.count == capacity) {
                throw std::runtime_error("Buffer overflow");
            }
            vwap_window.push(inferred_trades[i]);

            vwap_window.Trades[vwap_window.head] = inferred_trades[i];
            vwap_window.head = (vwap_window.head + 1) & (capacity - 1);
            ++vwap_window.count;
        }

        *book = L1Book{
            .bid_price = quote->bid_price,
            .bid_qty = quote->bid_qty,
            .ask_price = quote->ask_price,
            .ask_qty = quote->ask_qty,
        };

        //update vwap
        return inferred_count != 0;
    }
    else if (auto* trade = std::get_if<Trade>(&event.payload)) {
        (void)trade;
        //checkConstraints();
        //updateVwap()
    }

    return false;
}
