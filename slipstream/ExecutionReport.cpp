#include "ExecutionReport.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string FormatPrice(const std::int64_t price) {
    const bool negative = price < 0;
    const std::uint64_t magnitude = negative
        ? static_cast<std::uint64_t>(-(price + 1)) + 1
        : static_cast<std::uint64_t>(price);

    std::ostringstream output;
    if (negative) {
        output << '-';
    }

    output
        << magnitude / 10'000
        << '.'
        << std::setw(4)
        << std::setfill('0')
        << magnitude % 10'000;

    return output.str();
}

}

TickToOrderStatistics CalculateTickToOrderStatistics(
    const std::span<const std::uint64_t> samples) {
    if (samples.empty()) {
        return {};
    }

    std::vector<std::uint64_t> sorted_samples{
        samples.begin(),
        samples.end()};
    std::sort(sorted_samples.begin(), sorted_samples.end());

    const auto percentile = [&sorted_samples](
        const std::size_t numerator,
        const std::size_t denominator) {
        const std::size_t rank =
            (sorted_samples.size() * numerator + denominator - 1) /
            denominator;
        return sorted_samples[rank - 1];
    };

    return {
        .p50_ns = percentile(50, 100),
        .p99_ns = percentile(99, 100),
        .p999_ns = percentile(999, 1'000),
        .sample_count = sorted_samples.size(),
    };
}

std::string FormatExecutionReport(
    const ExecutionReport& report,
    const SlipstreamConfig& config) {
    const double execution_percent = report.submitted_qty == 0
        ? 0.0
        : 100.0 * static_cast<double>(report.executed_qty) /
          static_cast<double>(report.submitted_qty);

    const double participation_percent = report.market_qty == 0
        ? 0.0
        : 100.0 * static_cast<double>(report.executed_qty) /
          static_cast<double>(report.market_qty);

    const std::int64_t average_fill_price = report.executed_qty == 0
        ? 0
        : static_cast<std::int64_t>(
            report.executed_pq_sum / report.executed_qty);

    const std::int64_t session_vwap = report.market_qty == 0
        ? 0
        : static_cast<std::int64_t>(
            report.market_pq_sum / report.market_qty);

    double slippage_bps = 0.0;
    if (report.executed_qty != 0 && session_vwap != 0) {
        const __int128_t buy_cost =
            report.buy_pq_sum -
            static_cast<__int128_t>(session_vwap) * report.buy_qty;
        const __int128_t sell_cost =
            static_cast<__int128_t>(session_vwap) * report.sell_qty -
            report.sell_pq_sum;
        const __int128_t denominator =
            static_cast<__int128_t>(session_vwap) *
            report.executed_qty;

        slippage_bps =
            static_cast<double>(buy_cost + sell_cost) *
            10'000.0 /
            static_cast<double>(denominator);
    }

    const char* slippage_label = slippage_bps < 0.0
        ? "favorable"
        : slippage_bps > 0.0
            ? "unfavorable"
            : "neutral";

    std::ostringstream output;
    output << "=== SLIPSTREAM EXECUTION REPORT ===\n";
    output << std::left << std::setw(20) << "symbol"
           << config.symbol << '\n';
    output << std::left << std::setw(20) << "market qty"
           << report.market_qty << '\n';
    output << std::left << std::setw(20) << "executed qty"
           << report.executed_qty
           << "   (" << std::fixed << std::setprecision(2)
           << execution_percent << "%)\n";
    output << std::left << std::setw(20) << "avg fill price"
           << FormatPrice(average_fill_price) << '\n';
    output << std::left << std::setw(20) << "session VWAP"
           << FormatPrice(session_vwap) << '\n';
    output << std::left << std::setw(20) << "slippage vs VWAP"
           << std::fixed << std::setprecision(2)
           << slippage_bps << " bps   ("
           << slippage_label << ")\n";
    output << std::left << std::setw(20) << "participation"
           << std::fixed << std::setprecision(2)
           << participation_percent
           << "%  (cap "
           << config.participation_cap * 100.0
           << "%)\n";

    if (report.tick_to_order.sample_count == 0) {
        output << std::left << std::setw(20) << "tick-to-order p50"
               << "n/a\n";
        output << std::left << std::setw(20) << "tick-to-order p99"
               << "n/a\n";
        output << std::left << std::setw(20) << "tick-to-order p99.9"
               << "n/a\n";
    } else {
        output << std::left << std::setw(20) << "tick-to-order p50"
               << std::fixed << std::setprecision(3)
               << static_cast<double>(report.tick_to_order.p50_ns) / 1'000.0
               << " us\n";
        output << std::left << std::setw(20) << "tick-to-order p99"
               << std::fixed << std::setprecision(3)
               << static_cast<double>(report.tick_to_order.p99_ns) / 1'000.0
               << " us\n";
        output << std::left << std::setw(20) << "tick-to-order p99.9"
               << std::fixed << std::setprecision(3)
               << static_cast<double>(report.tick_to_order.p999_ns) / 1'000.0
               << " us\n";
    }
    output << std::left << std::setw(20) << "latency samples"
           << report.tick_to_order.sample_count << '\n';

    return output.str();
}
