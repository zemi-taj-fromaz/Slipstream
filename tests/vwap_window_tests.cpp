#include "VwapWindow.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace {

constexpr std::int64_t market_price = 1'000'000;
constexpr std::uint64_t window_ns = 1'000'000'000ULL;

TradePrint MakePrint(const std::int64_t price,
                     const std::uint32_t qty,
                     const std::uint64_t ts_ns,
                     const TradeOrigin origin,
                     const TradeSide side = TradeSide::Unknown) {
    return TradePrint{
        .pq = static_cast<__int128_t>(price) * qty,
        .ts_ns = ts_ns,
        .price = price,
        .qty = qty,
        .origin = origin,
        .side = side,
    };
}

VwapWindow MakeWarmWindow() {
    SlipstreamConfig config{};
    config.vwap_window_ms = 1'000;
    config.max_quantity = 500;
    config.participation_cap = 1.0;
    config.band_bps = 25.5;

    VwapWindow window{config};
    for (std::uint32_t i = 0; i < 10; ++i) {
        const std::uint64_t ts = i == 9
            ? window_ns
            : static_cast<std::uint64_t>(i) * 100'000'000ULL;
        const TradeDecision decision = window.push(
            MakePrint(
                market_price,
                1'000,
                ts,
                TradeOrigin::Market));
        EXPECT_EQ(
            decision.result,
            TradeResult::MarketTradeRecorded);
    }

    return window;
}

TEST(VwapWindowBands, AssignsBuyToUnknownTradeBelowVwap) {
    VwapWindow window = MakeWarmWindow();

    const TradeDecision decision = window.push(
        MakePrint(
            997'000,
            100,
            window_ns,
            TradeOrigin::User));

    EXPECT_EQ(decision.result, TradeResult::UserTradeAccepted);
    EXPECT_EQ(decision.side, TradeSide::Buy);
}

TEST(VwapWindowBands, AssignsSellToUnknownTradeAboveVwap) {
    VwapWindow window = MakeWarmWindow();

    const TradeDecision decision = window.push(
        MakePrint(
            1'003'000,
            100,
            window_ns,
            TradeOrigin::User));

    EXPECT_EQ(decision.result, TradeResult::UserTradeAccepted);
    EXPECT_EQ(decision.side, TradeSide::Sell);
}

TEST(VwapWindowBands, KeepsKnownBuyAndRejectsWrongPrice) {
    VwapWindow window = MakeWarmWindow();

    const TradeDecision decision = window.push(
        MakePrint(
            1'003'000,
            100,
            window_ns,
            TradeOrigin::User,
            TradeSide::Buy));

    EXPECT_EQ(
        decision.result,
        TradeResult::UserTradeRejectedBand);
    EXPECT_EQ(decision.side, TradeSide::Buy);
}

TEST(VwapWindowBands, KeepsKnownSellAndRejectsWrongPrice) {
    VwapWindow window = MakeWarmWindow();

    const TradeDecision decision = window.push(
        MakePrint(
            997'000,
            100,
            window_ns,
            TradeOrigin::User,
            TradeSide::Sell));

    EXPECT_EQ(
        decision.result,
        TradeResult::UserTradeRejectedBand);
    EXPECT_EQ(decision.side, TradeSide::Sell);
}

TEST(VwapWindowBands, PreservesFractionalBasisPoints) {
    VwapWindow window = MakeWarmWindow();

    const TradeDecision decision = window.push(
        MakePrint(
            997'475,
            100,
            window_ns,
            TradeOrigin::User));

    EXPECT_EQ(
        decision.result,
        TradeResult::UserTradeRejectedBand);
    EXPECT_EQ(decision.side, TradeSide::Buy);
}

TEST(VwapWindowBands, UsesBuyAsEqualPriceTieBreaker) {
    VwapWindow window = MakeWarmWindow();

    const TradeDecision decision = window.push(
        MakePrint(
            market_price,
            100,
            window_ns,
            TradeOrigin::User));

    EXPECT_EQ(
        decision.result,
        TradeResult::UserTradeRejectedBand);
    EXPECT_EQ(decision.side, TradeSide::Buy);
}

}
