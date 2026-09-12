#include "ExecutionReport.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <ostream>
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

void TickToOrderHistogram::Record(const std::uint64_t latency_ns) noexcept {
    RecordHistogram(latency_ns);
    RecordRaw({
        .received_ns = 0,
        .send_started_ns = latency_ns,
        .trade_id = 0,
    });
}

void TickToOrderHistogram::RecordHistogram(
    const std::uint64_t latency_ns) noexcept {
    const std::size_t bucket = static_cast<std::size_t>(
        latency_ns / bucket_width_ns);
    ++sample_count_;

    if (bucket >= buckets_.size()) {
        ++overflow_count_;
        return;
    }

    ++buckets_[bucket];
}

void TickToOrderHistogram::RecordRaw(
    const TickToOrderSample sample) noexcept {
    if (raw_sample_count_ == raw_samples_.size()) {
        return;
    }
    raw_samples_[raw_sample_count_++] = sample;
}

void TickToOrderHistogram::Record(
    const std::uint64_t received_ns,
    const std::uint64_t send_started_ns,
    const std::int64_t trade_id) noexcept {
    if (send_started_ns < received_ns) {
        return;
    }

    RecordHistogram(send_started_ns - received_ns);
    RecordRaw({
        .received_ns = received_ns,
        .send_started_ns = send_started_ns,
        .trade_id = trade_id,
    });
}

TickToOrderStatistics
TickToOrderHistogram::GetStatistics() const noexcept {
    if (raw_sample_count_ == 0) {
        return {};
    }

    std::array<std::uint64_t, raw_sample_capacity> latencies{};
    for (std::size_t index = 0; index < raw_sample_count_; ++index) {
        latencies[index] =
            raw_samples_[index].send_started_ns -
            raw_samples_[index].received_ns;
    }
    std::sort(
        latencies.begin(),
        latencies.begin() + static_cast<std::ptrdiff_t>(raw_sample_count_));

    const auto percentile = [&latencies, this](
        const std::uint64_t numerator,
        const std::uint64_t denominator) {
        const std::uint64_t rank =
            (raw_sample_count_ * numerator + denominator - 1) /
            denominator;
        return latencies[static_cast<std::size_t>(rank - 1)];
    };

    return {
        .p50_ns = percentile(50, 100),
        .p99_ns = percentile(99, 100),
        .p999_ns = percentile(999, 1'000),
        .sample_count = raw_sample_count_,
        .overflow_count = overflow_count_,
    };
}

void TickToOrderHistogram::WriteRawCsv(
    std::ostream& output, std::string_view transport, ExecutionMode mode) const {
    output << "transport,execution_mode,trade_id,received_ns,send_started_ns,tto_ns\n";
    for (std::size_t index = 0; index < raw_sample_count_; ++index) {
        const TickToOrderSample& sample = raw_samples_[index];
        output
            << transport << ',' << ExecutionModeName(mode) << ','
            << sample.trade_id << ','
            << sample.received_ns << ','
            << sample.send_started_ns << ','
            << sample.send_started_ns - sample.received_ns << '\n';
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
    output << std::left << std::setw(20) << "execution mode"
           << ExecutionModeName(config.execution_mode) << '\n';
    output << std::left << std::setw(20) << "transport"
           << config.transport << '\n';
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
    output << std::left << std::setw(20) << "latency >= 5000 us"
           << report.tick_to_order.overflow_count << '\n';

    return output.str();
}
