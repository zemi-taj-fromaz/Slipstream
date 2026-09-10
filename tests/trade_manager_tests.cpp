#include "TradeManager.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <variant>

namespace {

MarketEvent MakeQuote(
    const std::uint64_t ts,
    const std::int64_t bid_price,
    const std::uint32_t bid_qty,
    const std::int64_t ask_price,
    const std::uint32_t ask_qty) {
    MarketEvent event{
        .ts = ts,
        .symbol = {},
        .payload = Quote{
            .bid_price = bid_price,
            .ask_price = ask_price,
            .bid_qty = bid_qty,
            .ask_qty = ask_qty,
        },
    };
    std::memcpy(event.symbol, "SYNTH1", 6);
    return event;
}

MarketEvent MakeTrade(
    const std::uint64_t ts,
    const std::int64_t price,
    const std::uint32_t qty,
    const char aggressor = '?') {
    MarketEvent event{
        .ts = ts,
        .symbol = {},
        .payload = Trade{
            .price = price,
            .id = 42,
            .qty = qty,
            .aggressor = aggressor,
        },
    };
    std::memcpy(event.symbol, "SYNTH1", 6);
    return event;
}

SlipstreamConfig MakeConfig() {
    SlipstreamConfig config{};
    config.vwap_window_ms = 1'000;
    config.max_quantity = 2'000;
    config.participation_cap = 1.0;
    config.band_bps = 0.0;
    return config;
}

void WarmUp(TradeManager& manager) {
    static_cast<void>(manager.Push(
        MakeQuote(0, 999'000, 1'000, 1'000'000, 1'000)));

    for (std::uint64_t index = 0; index < 10; ++index) {
        const std::uint64_t timestamp = index == 9
            ? 1'000'000'000ULL
            : index * 100'000'000ULL;
        static_cast<void>(manager.Push(MakeQuote(
            timestamp,
            999'000,
            1'000,
            1'000'100 + static_cast<std::int64_t>(index) * 100,
            1'000)));
    }
}

TEST(TradeManager, FirstQuoteInitializesBookWithoutInferringVolume) {
    TradeManager manager{MakeConfig()};

    const TradeManagerResult result = manager.Push(
        MakeQuote(0, 999'000, 500, 1'001'000, 600));

    ASSERT_TRUE(std::holds_alternative<MarketUpdateResult>(result));
    const auto& update = std::get<MarketUpdateResult>(result);
    EXPECT_EQ(update.market_qty_delta, 0U);
    EXPECT_EQ(update.market_pq_delta, 0);
}

TEST(TradeManager, InfersAskExecutionFromQuantityReduction) {
    TradeManager manager{MakeConfig()};
    static_cast<void>(manager.Push(
        MakeQuote(0, 999'000, 500, 1'001'000, 600)));

    const TradeManagerResult result = manager.Push(
        MakeQuote(1, 999'000, 500, 1'001'000, 450));

    ASSERT_TRUE(std::holds_alternative<MarketUpdateResult>(result));
    const auto& update = std::get<MarketUpdateResult>(result);
    EXPECT_EQ(update.market_qty_delta, 150U);
    EXPECT_EQ(
        update.market_pq_delta,
        static_cast<__int128_t>(1'001'000) * 150);
}

TEST(TradeManager, InfersBothSidesWhenTopLevelsMoveAway) {
    TradeManager manager{MakeConfig()};
    static_cast<void>(manager.Push(
        MakeQuote(0, 999'000, 500, 1'001'000, 600)));

    const TradeManagerResult result = manager.Push(
        MakeQuote(1, 998'000, 700, 1'002'000, 800));

    ASSERT_TRUE(std::holds_alternative<MarketUpdateResult>(result));
    const auto& update = std::get<MarketUpdateResult>(result);
    EXPECT_EQ(update.market_qty_delta, 1'100U);
    EXPECT_EQ(
        update.market_pq_delta,
        static_cast<__int128_t>(999'000) * 500 +
            static_cast<__int128_t>(1'001'000) * 600);
}

TEST(TradeManager, ReturnsNoOrderForUserTradeBeforeWarmup) {
    TradeManager manager{MakeConfig()};

    const TradeManagerResult result = manager.Push(
        MakeTrade(0, 997'000, 100));

    ASSERT_TRUE(std::holds_alternative<UserTradeResult>(result));
    EXPECT_EQ(
        std::get<UserTradeResult>(result).decision.result,
        TradeResult::NoOrder);
}

TEST(TradeManager, ProducesAcceptedDecisionAfterWarmup) {
    TradeManager manager{MakeConfig()};
    WarmUp(manager);

    const TradeManagerResult result = manager.Push(
        MakeTrade(1'000'000'000ULL, 997'000, 100));

    ASSERT_TRUE(std::holds_alternative<UserTradeResult>(result));
    const auto& user = std::get<UserTradeResult>(result);
    EXPECT_EQ(user.decision.result, TradeResult::UserTradeAccepted);
    EXPECT_EQ(user.decision.side, TradeSide::Buy);
    EXPECT_EQ(user.decision.submitted_qty, 100U);
    EXPECT_EQ(user.decision.executed_qty, 100U);
}

TEST(TradeManager, RejectsInvalidAggressor) {
    TradeManager manager{MakeConfig()};

    EXPECT_THROW(
        static_cast<void>(manager.Push(
            MakeTrade(0, 997'000, 100, 'X'))),
        std::runtime_error);
}

} // namespace
