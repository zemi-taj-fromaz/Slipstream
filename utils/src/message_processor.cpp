#include "message_processor.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>

void IProcessMsgClass::Sink(const MarketEvent&) {
}

namespace {

[[nodiscard]] std::string Symbol(const MarketEvent& event) {
    const auto end = std::find(
        std::begin(event.symbol),
        std::end(event.symbol),
        '\0');
    return {std::begin(event.symbol), end};
}

} // namespace

FileProcessMsg::FileProcessMsg(const char* path)
    : file_{path} {
    if (!file_) {
        throw std::runtime_error("failed to open message processing log file");
    }

    file_ << "event_ts_ns,wall_delta_ns,event_delta_ns,type,symbol,"
             "bid_price,bid_qty,ask_price,ask_qty,price,qty\n";
}

void FileProcessMsg::Sink(const MarketEvent& event) {
    const auto now = std::chrono::steady_clock::now();

    std::uint64_t wall_delta_ns = 0;
    std::uint64_t event_delta_ns = 0;
    if (has_last_event_) {
        wall_delta_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now - last_sink_)
                .count());

        if (event.ts >= last_event_timestamp_) {
            event_delta_ns = event.ts - last_event_timestamp_;
        }
    }

    file_ << event.ts << ','
          << wall_delta_ns << ','
          << event_delta_ns << ',';

    if (std::holds_alternative<Quote>(event.payload)) {
        const auto& quote = std::get<Quote>(event.payload);
        file_ << 'Q' << ','
              << Symbol(event) << ','
              << quote.bid_price << ','
              << quote.bid_qty << ','
              << quote.ask_price << ','
              << quote.ask_qty << ",,\n";
    } else {
        const auto& trade = std::get<Trade>(event.payload);
        file_ << 'T' << ','
              << Symbol(event) << ",,,,,"
              << trade.price << ','
              << trade.qty << '\n';
    }

    last_sink_ = now;
    last_event_timestamp_ = event.ts;
    has_last_event_ = true;
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
