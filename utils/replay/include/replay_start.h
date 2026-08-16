#ifndef SLIPSTREAM_REPLAY_START_H
#define SLIPSTREAM_REPLAY_START_H

#include <cstdint>

namespace utils {

[[nodiscard]] std::uint64_t ParseReplayStartNs(
    int argc,
    char* const argv[]);

} // namespace utils

#endif // SLIPSTREAM_REPLAY_START_H
