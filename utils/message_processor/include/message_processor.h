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

class IMsgController {
public:
    virtual ~IMsgController() = default;

    virtual void Sink(const MarketEvent& event);
    virtual utils::ConnectionResult Send(const MarketEvent& event);
    virtual utils::ConnectionResult ProcessInboundUntil(
        std::chrono::steady_clock::time_point deadline);
};


class CanonicalFileMsgController final : public IMsgController {
public:
    explicit CanonicalFileMsgController(const char* path);

    void Sink(const MarketEvent& event) override;

private:
    std::ofstream file_;
};

class ConsoleMsgController final : public IMsgController {
public:
    explicit ConsoleMsgController(std::string process_name = {});

    void Sink(const MarketEvent& event) override;

private:
    std::string process_name_;
};

class FanoutMsgController final : public IMsgController {
public:
    FanoutMsgController(
        IMsgController& first,
        IMsgController& second);

    void Sink(const MarketEvent& event) override;
    utils::ConnectionResult Send(const MarketEvent& event) override;

private:
    IMsgController& first_;
    IMsgController& second_;
};

enum class EventType {
    Quote = 0,
    Trade = 1,
};

template <EventType event_type>
utils::ConnectionResult ProcessRowsByTimestamp(
    const std::vector<MarketEvent>& rows,
    IMsgController& controller,
    std::uint64_t start_at_ns) {
    if (rows.empty()) {
        return utils::ConnectionResult::Complete;
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
        if constexpr (event_type == EventType::Quote) {
            if (!std::holds_alternative<Quote>(row.payload)) {
                continue;
            }
        } else {
            if (!std::holds_alternative<Trade>(row.payload)) {
                continue;
            }
        }

        if (row.ts < first_timestamp) {
            throw std::runtime_error("replay rows are not sorted by timestamp");
        }

        const std::uint64_t timestamp_delta = row.ts - first_timestamp;
        const auto send_at = replay_start + std::chrono::nanoseconds{
            static_cast<std::chrono::nanoseconds::rep>(timestamp_delta)};

        if constexpr (event_type == EventType::Trade) {
            if (controller.ProcessInboundUntil(send_at) ==
                utils::ConnectionResult::PeerDisconnected) {
                return utils::ConnectionResult::PeerDisconnected;
            }
        } else {
            std::this_thread::sleep_until(send_at);
        }

        if (controller.Send(row) ==
            utils::ConnectionResult::PeerDisconnected) {
            return utils::ConnectionResult::PeerDisconnected;
        }
    }

    return utils::ConnectionResult::Complete;
}

#endif // SLIPSTREAM_MESSAGE_PROCESSOR_H
