#ifndef SLIPSTREAM_EXECUTIONREPORT_H
#define SLIPSTREAM_EXECUTIONREPORT_H

#include "SlipstreamConfig.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

struct TickToOrderStatistics {
    std::uint64_t p50_ns{};
    std::uint64_t p99_ns{};
    std::uint64_t p999_ns{};
    std::size_t sample_count{};
};

struct ExecutionReport {
    std::uint64_t market_qty{};
    std::uint64_t submitted_qty{};
    std::uint64_t executed_qty{};

    __int128_t market_pq_sum{};
    __int128_t executed_pq_sum{};

    std::uint64_t buy_qty{};
    std::uint64_t sell_qty{};

    __int128_t buy_pq_sum{};
    __int128_t sell_pq_sum{};

    TickToOrderStatistics tick_to_order{};
};

[[nodiscard]]
TickToOrderStatistics CalculateTickToOrderStatistics(
    std::span<const std::uint64_t> samples);

[[nodiscard]]
std::string FormatExecutionReport(
    const ExecutionReport& report,
    const SlipstreamConfig& config);

#endif
