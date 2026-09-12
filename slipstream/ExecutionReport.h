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

struct TickToOrderSample {
    std::uint64_t received_ns{};
    std::uint64_t send_started_ns{};
    std::int64_t trade_id{};
};

class TickToOrderHistogram {
public:
    static constexpr std::uint64_t bucket_width_ns = 1'000'000;
    static constexpr std::size_t bucket_count = 5;
    static constexpr std::size_t raw_sample_capacity = 2'048;

    void Record(std::uint64_t latency_ns) noexcept;
    void Record(std::uint64_t received_ns, std::uint64_t send_started_ns, std::int64_t trade_id) noexcept;

    [[nodiscard]] TickToOrderStatistics GetStatistics() const noexcept;

    void WriteRawCsv(std::ostream& output, std::string_view transport = "tcp",
                     ExecutionMode mode = ExecutionMode::Wait) const;

private:
    void RecordHistogram(std::uint64_t latency_ns) noexcept;
    void RecordRaw(TickToOrderSample sample) noexcept;

    std::array<std::uint32_t, bucket_count> buckets_{};
    std::array<TickToOrderSample, raw_sample_capacity> raw_samples_{};
    std::size_t raw_sample_count_{};
    std::uint64_t overflow_count_{};
    std::uint64_t sample_count_{};
};

struct ExecutionReport {
    __int128_t market_pq_sum{};
    __int128_t executed_pq_sum{};
    __int128_t buy_pq_sum{};
    __int128_t sell_pq_sum{};

    std::uint64_t market_qty{};
    std::uint64_t submitted_qty{};
    std::uint64_t executed_qty{};
    std::uint64_t buy_qty{};
    std::uint64_t sell_qty{};

    TickToOrderStatistics tick_to_order{};
};

[[nodiscard]] TickToOrderStatistics CalculateTickToOrderStatistics(std::span<const std::uint64_t> samples);

[[nodiscard]] std::string FormatExecutionReport(const ExecutionReport& report, const SlipstreamConfig& config);

#endif
