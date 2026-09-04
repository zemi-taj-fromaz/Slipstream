#ifndef SLIPSTREAM_OE_LATENCY_RECORDER_H
#define SLIPSTREAM_OE_LATENCY_RECORDER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class OeLatencyRecorder {
public:
    OeLatencyRecorder(std::string transport, std::string output_path);

    void RecordSend(std::int64_t trade_id);
    void RecordConfirmation(std::int64_t trade_id, char status);
    void WriteCsv() const;

    [[nodiscard]] std::size_t SampleCount() const noexcept;
    [[nodiscard]] const std::string& OutputPath() const noexcept;

private:
    struct Sample {
        std::int64_t trade_id{};
        char status{};
        std::uint64_t send_ns{};
        std::uint64_t receive_ns{};
        std::uint64_t latency_ns{};
    };

    [[nodiscard]] static std::uint64_t NowNs() noexcept;

    std::string transport_;
    std::string output_path_;
    std::unordered_map<std::int64_t, std::uint64_t> pending_sends_;
    std::vector<Sample> samples_;
};

#endif
