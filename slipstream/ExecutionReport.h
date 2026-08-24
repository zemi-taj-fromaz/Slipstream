#ifndef SLIPSTREAM_EXECUTIONREPORT_H
#define SLIPSTREAM_EXECUTIONREPORT_H

#include "SlipstreamConfig.h"

#include <cstdint>
#include <string>

struct ExecutionReport {
    std::uint64_t market_qty{};
    std::uint64_t submitted_qty{};
    std::uint64_t executed_qty{};

    __int128_t market_notional_sum{};
    __int128_t executed_notional_sum{};

    std::uint64_t buy_qty{};
    std::uint64_t sell_qty{};

    __int128_t buy_notional_sum{};
    __int128_t sell_notional_sum{};
};

[[nodiscard]]
std::string FormatExecutionReport(
    const ExecutionReport& report,
    const SlipstreamConfig& config);

#endif
