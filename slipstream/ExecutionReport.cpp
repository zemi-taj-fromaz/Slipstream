#include "ExecutionReport.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

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
            report.executed_notional_sum / report.executed_qty);

    const std::int64_t session_vwap = report.market_qty == 0
        ? 0
        : static_cast<std::int64_t>(
            report.market_notional_sum / report.market_qty);

    double slippage_bps = 0.0;
    if (report.executed_qty != 0 && session_vwap != 0) {
        const __int128_t buy_cost =
            report.buy_notional_sum -
            static_cast<__int128_t>(session_vwap) * report.buy_qty;
        const __int128_t sell_cost =
            static_cast<__int128_t>(session_vwap) * report.sell_qty -
            report.sell_notional_sum;
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

    return output.str();
}
