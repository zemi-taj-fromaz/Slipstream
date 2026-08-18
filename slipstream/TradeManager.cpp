//
// Created by babodev on 18.08.2026..
//

#include "TradeManager.h"

TradeManager::TradeManager(const SlipstreamConfig& slipstream) : max_qty(slipstream.max_quantity), participation_cap(slipstream.participation_cap), vwap_window_ms(slipstream.vwap_window_ms),band_bps(slipstream.band_bps)
{
}