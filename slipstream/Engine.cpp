//
// Created by babodev on 16.08.2026..
//

#include "Engine.h"

#include "../../../Library/Caches/JetBrains/CLion2026.1/.docker/2026_1/Docker/slipstream-dev_gcc14/usr/local/include/c++/14.3.0/atomic"

Engine::Engine(const Slipstream& slipstream,
               rigtorp::SPSCQueue<MarketEvent>& in,
               rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out) : max_qty(slipstream.max_qty), participation_cap(slipstream.participation_cap), vwap_window_ms(slipstream.vwap_window_ms),band_bps(slipstream.band_bps)
                                                                          , ingress(in), egress(out)
{
}

void Engine::Run() {

    while (running.load(std::memory_order_relaxed)) {
        if (auto* front = ingress.front()) {

        }
    }

}