#ifndef SLIPSTREAM_MESSAGE_PROCESSOR_H
#define SLIPSTREAM_MESSAGE_PROCESSOR_H

#include "market_event.h"
#include "socket.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class IProcessMsgClass {
public:
    virtual ~IProcessMsgClass() = default;

    virtual void Sink(const MarketEvent& event);
};


class FileProcessMsg final : public IProcessMsgClass {
public:
    explicit FileProcessMsg(const char* path);

    void Sink(const MarketEvent& event) override;

private:
    std::ofstream file_;
    std::chrono::steady_clock::time_point last_sink_{};
    std::uint64_t last_event_timestamp_{0};
    bool has_last_event_{false};
};

class ConsoleProcessMsg final : public IProcessMsgClass {
public:
    explicit ConsoleProcessMsg(std::string process_name = {});

    void Sink(const MarketEvent& event) override;

private:
    std::string process_name_;
};

class FanoutProcessMsg final : public IProcessMsgClass {
public:
    FanoutProcessMsg(
        IProcessMsgClass& first,
        IProcessMsgClass& second);

    void Sink(const MarketEvent& event) override;

private:
    IProcessMsgClass& first_;
    IProcessMsgClass& second_;
};

enum class EventType {
    Quote = 0,
    Trade = 1,
};

template <EventType event_type>
void ProcessRowsByTimestamp(
    const std::vector<MarketEvent>& rows,
    IProcessMsgClass& processor,
    std::uint64_t start_at_ns) {
    if (rows.empty()) {
        return;
    }

    const auto system_now = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();
    using NanosecondsRep = std::chrono::nanoseconds::rep;

    if (start_at_ns >
        static_cast<std::uint64_t>(std::numeric_limits<NanosecondsRep>::max())) {
        throw std::invalid_argument("replay start time is out of range");
    }

    const auto start_at = std::chrono::system_clock::time_point{
        std::chrono::nanoseconds{static_cast<NanosecondsRep>(start_at_ns)}};

    if (start_at <= system_now) {
        throw std::runtime_error("replay start time has already passed");
    }

    const auto replay_start = steady_now +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            start_at - system_now);
    const std::uint64_t first_timestamp = rows.front().ts;

    for (const MarketEvent& row : rows) {
        if (row.ts > first_timestamp) {
            const std::uint64_t timestamp_delta = row.ts - first_timestamp;
            const auto delay = std::chrono::nanoseconds{
                static_cast<std::chrono::nanoseconds::rep>(timestamp_delta)};

            std::this_thread::sleep_until(replay_start + delay);
        }
        if (std::holds_alternative<Quote>(row.payload) && event_type == EventType::Quote
            || std::holds_alternative<Trade>(row.payload) && event_type == EventType::Trade) {
            processor.Sink(row);
        }
    }
}

#endif // SLIPSTREAM_MESSAGE_PROCESSOR_H
