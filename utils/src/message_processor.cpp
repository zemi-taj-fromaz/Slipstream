#include "message_processor.h"

#include <chrono>
#include <thread>

void IProcessMsgClass::Sink(const MarketEvent&) {
}

void ProcessRowsByTimestamp(
    const std::vector<MarketEvent>& rows,
    IProcessMsgClass& processor) {
    if (rows.empty()) {
        return;
    }

    const auto replay_start = std::chrono::steady_clock::now();
    const std::uint64_t first_timestamp = rows.front().ts;

    for (const MarketEvent& row : rows) {
        if (row.ts > first_timestamp) {
            const std::uint64_t timestamp_delta = row.ts - first_timestamp;
            const auto delay = std::chrono::nanoseconds{
                static_cast<std::chrono::nanoseconds::rep>(timestamp_delta)};

            std::this_thread::sleep_until(replay_start + delay);
        }

        processor.Sink(row);
    }
}
