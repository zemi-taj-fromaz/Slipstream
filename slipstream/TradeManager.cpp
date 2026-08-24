//
// Created by babodev on 18.08.2026..
//

#include "TradeManager.h"

#include <array>
#include <stdexcept>

namespace {

TradeSide ToTradeSide(const char aggressor) {
    switch (aggressor) {
    case 'B':
        return TradeSide::Buy;
    case 'S':
        return TradeSide::Sell;
    case '?':
        return TradeSide::Unknown;
    default:
        throw std::runtime_error("invalid trade aggressor");
    }
}

}

TradeManager::TradeManager(const SlipstreamConfig& slipstream) : vwap_window(slipstream)
{

}

TradeManagerResult TradeManager::Push(MarketEvent& event) {
    if (auto* quote = std::get_if<Quote>(&event.payload)) {
        if (!book) {
            book = std::make_unique<L1Book>(L1Book{
                .bid_price = quote->bid_price,
                .bid_qty = quote->bid_qty,
                .ask_price = quote->ask_price,
                .ask_qty = quote->ask_qty,
            });
            return MarketUpdateResult{};
        }

        std::array<TradePrint, 2> inferred_trades{};
        std::size_t inferred_count = 0;

        const auto infer_trade = [&](const std::int64_t price,
                                     const std::uint32_t qty,
                                     const TradeSide side) {
            if (qty == 0) {
                return;
            }

            inferred_trades[inferred_count++] = TradePrint{
                .notional = static_cast<__int128_t>(price) * qty,
                .ts_ns = event.ts,
                .price = price,
                .qty = qty,
                .origin = TradeOrigin::Market,
                .side = side,
            };
        };

        if (quote->ask_price > book->ask_price) {
            infer_trade(
                book->ask_price,
                book->ask_qty,
                TradeSide::Buy);
        } else if (quote->ask_price == book->ask_price &&
                   quote->ask_qty < book->ask_qty) {
            infer_trade(
                book->ask_price,
                book->ask_qty - quote->ask_qty,
                TradeSide::Buy);
        }

        if (quote->bid_price < book->bid_price) {
            infer_trade(
                book->bid_price,
                book->bid_qty,
                TradeSide::Sell);
        } else if (quote->bid_price == book->bid_price &&
                   quote->bid_qty < book->bid_qty) {
            infer_trade(
                book->bid_price,
                book->bid_qty - quote->bid_qty,
                TradeSide::Sell);
        }

        MarketUpdateResult result{};
        for (std::size_t i = 0; i < inferred_count; ++i) {
            vwap_window.push(inferred_trades[i]);
            result.market_notional_delta +=
                inferred_trades[i].notional;
            result.market_qty_delta +=
                inferred_trades[i].qty;
        }

        *book = L1Book{
            .bid_price = quote->bid_price,
            .bid_qty = quote->bid_qty,
            .ask_price = quote->ask_price,
            .ask_qty = quote->ask_qty,
        };

        return result;
    }
    else if (auto* trade = std::get_if<Trade>(&event.payload)) {
        const TradeDecision decision = vwap_window.push(TradePrint{
                .notional =
                    static_cast<__int128_t>(trade->price) * trade->qty,
                .ts_ns = event.ts,
                .price = trade->price,
                .qty = trade->qty,
                .origin = TradeOrigin::User,
                .side = ToTradeSide(trade->aggressor),
            });

        return UserTradeResult{
            .decision = decision,
            .rolling_vwap = vwap_window.RollingVwap(),
        };
    }

    return MarketUpdateResult{};
}
