#ifndef SLIPSTREAM_REPLAY_START_H
#define SLIPSTREAM_REPLAY_START_H

#include <cstdint>
#include <string>

namespace utils {

struct ReplayClientOptions {
    std::string host;
    std::string transport{"tcp"};
    std::uint16_t port;
    std::uint64_t start_at_ns;
    std::string md_a_group{"239.255.0.1"};
    std::uint16_t md_a_port{14'200};
    std::string md_b_group{"239.255.0.2"};
    std::uint16_t md_b_port{14'201};
    std::string md_multicast_interface{"0.0.0.0"};
};

[[nodiscard]] ReplayClientOptions ParseReplayClientOptions(
    int argc,
    char* const argv[]);

[[nodiscard]] bool ReplayVerificationEnabled() noexcept;

} // namespace utils

#endif // SLIPSTREAM_REPLAY_START_H
