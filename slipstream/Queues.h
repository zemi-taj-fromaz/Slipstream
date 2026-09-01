#ifndef SLIPSTREAM_QUEUES_H
#define SLIPSTREAM_QUEUES_H

#include "market_event.h"
#include "slipstream_codec/market_data_codec.h"
#include "spsc_queue.h"

#include <cstddef>
#include <cstdint>

namespace slipstream {

inline constexpr std::size_t queue_capacity = 1024;

struct InboundEvent {
    MarketEvent message{};
    std::uint64_t received_at_ns{};
};

struct OutboundMessage {
    codec::OrderEntryClientMessage message{};
    std::uint64_t trigger_received_at_ns{};
    bool measure_tick_to_order{};
};

using MarketEventQueue = utils::spsc_queue<InboundEvent, queue_capacity>;
using OrderEntryQueue = utils::spsc_queue<OutboundMessage, queue_capacity>;

} // namespace slipstream

#endif // SLIPSTREAM_QUEUES_H
