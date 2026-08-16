#include "replay_start.h"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace utils {

std::uint64_t ParseReplayStartNs(
    int argc,
    char* const argv[]) {
    constexpr std::string_view option = "--start-at-ns";

    if (argc != 3 || std::string_view{argv[1]} != option) {
        throw std::invalid_argument(
            "usage: " + std::string{argv[0]} +
            " --start-at-ns <unix-nanoseconds>");
    }

    const std::string_view value_text{argv[2]};
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(
        value_text.data(),
        value_text.data() + value_text.size(),
        value);

    if (error != std::errc{} ||
        end != value_text.data() + value_text.size()) {
        throw std::invalid_argument(
            "--start-at-ns must be an unsigned integer");
    }

    return value;
}

} // namespace utils
