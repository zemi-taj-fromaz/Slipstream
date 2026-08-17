#ifndef SLIPSTREAM_REPLAY_START_H
#define SLIPSTREAM_REPLAY_START_H

#include <cstdint>
#include <string>

namespace utils {

struct ReplayClientOptions {
    std::string host;
    std::uint16_t port;
    std::uint64_t start_at_ns;
};

[[nodiscard]] ReplayClientOptions ParseReplayClientOptions(
    int argc,
    char* const argv[]);

[[nodiscard]] bool ReplayVerificationEnabled() noexcept;

} // namespace utils

#endif // SLIPSTREAM_REPLAY_START_H
