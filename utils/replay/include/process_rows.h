#ifndef SLIPSTREAM_PROCESS_ROWS_H
#define SLIPSTREAM_PROCESS_ROWS_H

#include "client_transport.h"
#include "market_event.h"
#include "message_processor.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <variant>
#include <vector>

enum class EventType {
    Quote = 0,
    Trade = 1,
};

template <EventType event_type>
utils::ConnectionResult ProcessRowsByTimestamp(
    const std::vector<MarketEvent>& rows,
    IClientTransport& transport,
    std::uint64_t start_at_ns,
    IMsgController* observer = nullptr) {
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

        if (transport.ProcessInboundUntil(send_at) ==
            utils::ConnectionResult::PeerDisconnected) {
            return utils::ConnectionResult::PeerDisconnected;
        }

        if (observer != nullptr) {
            observer->Sink(row);
        }

        if (transport.Send(row) ==
            utils::ConnectionResult::PeerDisconnected) {
            return utils::ConnectionResult::PeerDisconnected;
        }
    }

    return utils::ConnectionResult::Complete;
}

#endif
