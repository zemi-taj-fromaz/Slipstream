//
// Created by babodev on 17.08.2026..
//

#ifndef SLIPSTREAM_SLIPSTREAMCONFIG_H
#define SLIPSTREAM_SLIPSTREAMCONFIG_H

#include <cstdint>
#include <string>

struct SlipstreamConfig {
    std::string symbol{"SYNTH1"};
    std::uint32_t max_quantity{500};
    double participation_cap{0.15};
    std::uint32_t vwap_window_ms{30'000};
    double band_bps{25.5};

    std::string md_host{"127.0.0.1"};
    std::uint16_t md_port{14'200};

    std::string md_a_group{"239.255.0.1"};
    std::uint16_t md_a_port{14'200};
    std::string md_b_group{"239.255.0.2"};
    std::uint16_t md_b_port{14'201};
    std::string md_multicast_interface{"0.0.0.0"};

    std::string oe_host{"127.0.0.1"};
    std::uint16_t oe_port{14'300};

    std::string transport{"tcp"};
};

[[nodiscard]]
SlipstreamConfig ParseSlipstreamConfig(
    int argc,
    char* argv[]);

#endif //SLIPSTREAM_SLIPSTREAMCONFIG_H
