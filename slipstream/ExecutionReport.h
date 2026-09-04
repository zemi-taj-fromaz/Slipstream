#ifndef SLIPSTREAM_EXECUTIONREPORT_H
#define SLIPSTREAM_EXECUTIONREPORT_H

#include "SlipstreamConfig.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>

struct TickToOrderStatistics {
    std::uint64_t p50_ns{};
    std::uint64_t p99_ns{};
    std::uint64_t p999_ns{};
    std::size_t sample_count{};
    std::uint64_t overflow_count{};
};

class TickToOrderHistogram {
public:
    static constexpr std::uint64_t bucket_width_ns = 1'000;
    static constexpr std::size_t bucket_count = 5'000;

    void Record(std::uint64_t latency_ns) noexcept;

    [[nodiscard]]
    TickToOrderStatistics GetStatistics() const noexcept;

    void WriteCsv(std::ostream& output) const;

private:
    std::array<std::uint32_t, bucket_count> buckets_{};
    std::uint64_t overflow_count_{};
    std::uint64_t sample_count_{};
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
