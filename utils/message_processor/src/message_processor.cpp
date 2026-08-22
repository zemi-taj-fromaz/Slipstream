#include "message_processor.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>

void IMsgController::Sink(const MarketEvent&) {
}

void IMsgController::Send(const MarketEvent& event) {
    Sink(event);
}

void IMsgController::ProcessInboundUntil(
    std::chrono::steady_clock::time_point deadline) {
    std::this_thread::sleep_until(deadline);
}

CanonicalFileMsgController::CanonicalFileMsgController(const char* path)
    : file_{path} {
    if (!file_) {
        throw std::runtime_error("failed to open canonical event file");
    }

    file_ << "event_ts_ns,type,symbol,bid_price,bid_qty,ask_price,ask_qty,"
             "price,qty,aggressor,id\n";
}

void CanonicalFileMsgController::Sink(const MarketEvent& event) {
    file_ << event.ts << ',';

    if (std::holds_alternative<Quote>(event.payload)) {
        const auto& quote = std::get<Quote>(event.payload);
        file_ << 'Q' << ','
              << event.symbol << ','
              << quote.bid_price << ','
              << quote.bid_qty << ','
              << quote.ask_price << ','
              << quote.ask_qty << ",,,,\n";
        return;
    }

    const auto& trade = std::get<Trade>(event.payload);
    file_ << 'T' << ','
          << event.symbol << ",,,,,"
          << trade.price << ','
          << trade.qty << ','
          << trade.aggressor << ','
          << trade.id << '\n';
}

ConsoleMsgController::ConsoleMsgController(std::string process_name)
    : process_name_{std::move(process_name)} {
}

void ConsoleMsgController::Sink(const MarketEvent& event) {
    if (!process_name_.empty()) {
        std::cout << '[' << process_name_ << "] ";
    }

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

FanoutMsgController::FanoutMsgController(
    IMsgController& first,
    IMsgController& second)
    : first_{first},
      second_{second} {
}

void FanoutMsgController::Sink(const MarketEvent& event) {
    first_.Sink(event);
    second_.Sink(event);
}

void FanoutMsgController::Send(const MarketEvent& event) {
    first_.Send(event);
    second_.Send(event);
}
