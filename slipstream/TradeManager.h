//
// Created by babodev on 18.08.2026..
//

#ifndef SLIPSTREAM_TRADEMANAGER_H
#define SLIPSTREAM_TRADEMANAGER_H

#include <cstdint>
#include "SlipstreamConfig.h"


class TradeManager {
public:
    TradeManager(const SlipstreamConfig& slipstream);


private:
    std::uint32_t max_qty;
    double participation_cap;
    std::uint32_t vwap_window_ms;
    std::uint32_t band_bps;
};

#endif //SLIPSTREAM_TRADEMANAGER_H
