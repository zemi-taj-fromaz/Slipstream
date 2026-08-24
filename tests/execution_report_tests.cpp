#include "ExecutionReport.h"

#include <gtest/gtest.h>

#include <string>

TEST(ExecutionReport, FormatsBuyOnlySession) {
    ExecutionReport report{
        .market_qty = 400'000,
        .submitted_qty = 50'000,
        .executed_qty = 47'300,
        .market_notional_sum =
            static_cast<__int128_t>(1'012'701) * 400'000,
        .executed_notional_sum =
            static_cast<__int128_t>(1'012'438) * 47'300,
        .buy_qty = 47'300,
        .sell_qty = 0,
        .buy_notional_sum =
            static_cast<__int128_t>(1'012'438) * 47'300,
        .sell_notional_sum = 0,
    };
    SlipstreamConfig config{};
    config.symbol = "SYNTH1";
    config.participation_cap = 0.15;

    const std::string output =
        FormatExecutionReport(report, config);

    EXPECT_NE(
        output.find("executed qty        47300   (94.60%)"),
        std::string::npos);
    EXPECT_NE(
        output.find("avg fill price      101.2438"),
        std::string::npos);
    EXPECT_NE(
        output.find("session VWAP        101.2701"),
        std::string::npos);
    EXPECT_NE(
        output.find("slippage vs VWAP    -2.60 bps   (favorable)"),
        std::string::npos);
}

TEST(ExecutionReport, TreatsSellAboveVwapAsFavorable) {
    ExecutionReport report{
        .market_qty = 10'000,
        .submitted_qty = 100,
        .executed_qty = 100,
        .market_notional_sum =
            static_cast<__int128_t>(1'000'000) * 10'000,
        .executed_notional_sum =
            static_cast<__int128_t>(1'010'000) * 100,
        .buy_qty = 0,
        .sell_qty = 100,
        .buy_notional_sum = 0,
        .sell_notional_sum =
            static_cast<__int128_t>(1'010'000) * 100,
    };
    SlipstreamConfig config{};

    const std::string output =
        FormatExecutionReport(report, config);

    EXPECT_NE(
        output.find("slippage vs VWAP    -100.00 bps   (favorable)"),
        std::string::npos);
}

TEST(ExecutionReport, HandlesEmptySession) {
    const ExecutionReport report{};
    const SlipstreamConfig config{};

    const std::string output =
        FormatExecutionReport(report, config);

    EXPECT_NE(
        output.find("executed qty        0   (0.00%)"),
        std::string::npos);
    EXPECT_NE(
        output.find("avg fill price      0.0000"),
        std::string::npos);
    EXPECT_NE(
        output.find("session VWAP        0.0000"),
        std::string::npos);
    EXPECT_NE(
        output.find("slippage vs VWAP    0.00 bps   (neutral)"),
        std::string::npos);
}
