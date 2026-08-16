#include "message_processor.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <variant>

void IProcessMsgClass::Sink(const MarketEvent&) {
}

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
              << event.symbol << ','
              << quote.bid_price << ','
              << quote.bid_qty << ','
              << quote.ask_price << ','
              << quote.ask_qty << ",,\n";
    } else {
        const auto& trade = std::get<Trade>(event.payload);
        file_ << 'T' << ','
              << event.symbol << ",,,,,"
              << trade.price << ','
              << trade.qty << '\n';
    }

    last_sink_ = now;
    last_event_timestamp_ = event.ts;
    has_last_event_ = true;
}

void ConsoleProcessMsg::Sink(const MarketEvent& event) {
    std::cout << "ts=" << event.ts
              << " symbol=" << event.symbol;

    if (std::holds_alternative<Quote>(event.payload)) {
        const auto& quote = std::get<Quote>(event.payload);
        std::cout << " type=Q"
                  << " bid=" << quote.bid_price << 'x' << quote.bid_qty
                  << " ask=" << quote.ask_price << 'x' << quote.ask_qty;
    } else {
        const auto& trade = std::get<Trade>(event.payload);
        std::cout << " type=T"
                  << " price=" << trade.price
                  << " qty=" << trade.qty;
    }

    std::cout << '\n' << std::flush;
}

FanoutProcessMsg::FanoutProcessMsg(
    IProcessMsgClass& first,
    IProcessMsgClass& second)
    : first_{first},
      second_{second} {
}

void FanoutProcessMsg::Sink(const MarketEvent& event) {
    first_.Sink(event);
    second_.Sink(event);
}

