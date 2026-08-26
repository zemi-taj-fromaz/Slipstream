#ifndef SLIPSTREAM_QUEUES_H
#define SLIPSTREAM_QUEUES_H

#include "market_event.h"
#include "slipstream_codec/market_data_codec.h"
#include "spsc_queue.h"

#include <cstddef>

namespace slipstream {

inline constexpr std::size_t queue_capacity = 1024;

using MarketEventQueue = utils::spsc_queue<MarketEvent, queue_capacity>;
using OrderEntryQueue = utils::spsc_queue<codec::OrderEntryClientMessage, queue_capacity>;

} // namespace slipstream

#endif // SLIPSTREAM_QUEUES_H