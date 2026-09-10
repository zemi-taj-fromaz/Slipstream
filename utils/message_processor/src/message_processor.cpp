#include "message_processor.h"

#include <stdexcept>
#include <variant>

CanonicalFileEventObserver::CanonicalFileEventObserver(const char* path)
    : file_{path} {
    if (!file_) {
        throw std::runtime_error("failed to open canonical event file");
    }

    file_ << "event_ts_ns,type,symbol,bid_price,bid_qty,ask_price,ask_qty,"
             "price,qty,aggressor,id\n";
}

void CanonicalFileEventObserver::OnEvent(const MarketEvent& event) {
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
