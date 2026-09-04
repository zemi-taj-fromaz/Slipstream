#include "ExecutionReport.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

TEST(ExecutionReport, CalculatesNearestRankTickToOrderPercentiles) {
    std::vector<std::uint64_t> samples;
    samples.reserve(1'000);
    for (std::uint64_t sample = 1; sample <= 1'000; ++sample) {
        samples.push_back(sample);
    }

    const TickToOrderStatistics statistics =
        CalculateTickToOrderStatistics(samples);

    EXPECT_EQ(statistics.p50_ns, 500U);
    EXPECT_EQ(statistics.p99_ns, 990U);
    EXPECT_EQ(statistics.p999_ns, 999U);
    EXPECT_EQ(statistics.sample_count, 1'000U);
}

TEST(ExecutionReport, FormatsTickToOrderStatisticsInMicroseconds) {
    ExecutionReport report{};
    report.tick_to_order = {
        .p50_ns = 1'900,
        .p99_ns = 6'400,
        .p999_ns = 21'700,
        .sample_count = 100,
    };

    const std::string output =
        FormatExecutionReport(report, SlipstreamConfig{});

    EXPECT_NE(
        output.find("tick-to-order p50   1.900 us"),
        std::string::npos);
    EXPECT_NE(
        output.find("tick-to-order p99   6.400 us"),
        std::string::npos);
    EXPECT_NE(
        output.find("tick-to-order p99.9 21.700 us"),
        std::string::npos);
    EXPECT_NE(
        output.find("latency samples     100"),
        std::string::npos);
}

TEST(ExecutionReport, AggregatesTickToOrderHistogram) {
    TickToOrderHistogram histogram;
    histogram.Record(100);
    histogram.Record(1'500);
    histogram.Record(2'500);
    histogram.Record(6'000'000);

    const TickToOrderStatistics statistics = histogram.GetStatistics();

    EXPECT_EQ(statistics.p50_ns, 1'000U);
    EXPECT_EQ(statistics.p99_ns, 5'000'000U);
    EXPECT_EQ(statistics.p999_ns, 5'000'000U);
    EXPECT_EQ(statistics.sample_count, 4U);
    EXPECT_EQ(statistics.overflow_count, 1U);
}

TEST(ExecutionReport, WritesTickToOrderHistogramCsv) {
    TickToOrderHistogram histogram;
    histogram.Record(1'500);
    histogram.Record(6'000'000);

    std::ostringstream output;
    histogram.WriteCsv(output);

    EXPECT_NE(
        output.str().find("1,2,1,false"),
        std::string::npos);
    EXPECT_NE(
        output.str().find("5000,,1,true"),
        std::string::npos);
}

TEST(ExecutionReport, FormatsBuyOnlySession) {
    ExecutionReport report{
        .market_qty = 400'000,
        .submitted_qty = 50'000,
        .executed_qty = 47'300,
        .market_pq_sum =
            static_cast<__int128_t>(1'012'701) * 400'000,
        .executed_pq_sum =
            static_cast<__int128_t>(1'012'438) * 47'300,
        .buy_qty = 47'300,
        .sell_qty = 0,
        .buy_pq_sum =
            static_cast<__int128_t>(1'012'438) * 47'300,
        .sell_pq_sum = 0,
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
        .market_pq_sum =
            static_cast<__int128_t>(1'000'000) * 10'000,
        .executed_pq_sum =
            static_cast<__int128_t>(1'010'000) * 100,
        .buy_qty = 0,
        .sell_qty = 100,
        .buy_pq_sum = 0,
        .sell_pq_sum =
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
