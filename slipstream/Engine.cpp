//
// Created by babodev on 16.08.2026..
//

#include "Engine.h"

Engine::Engine(const Slipstream& slipstream,
        rigtorp::SPSCQueue<MarketEvent>& in,
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out) : max_qty(slipstream.max_qty), participation_cap(slipstream.participation_cap), vwap_window_ms(slipstream.vwap_window_ms),band_bps(slipstream.band_bps)
    , ingress(in), egress(out)
{
}