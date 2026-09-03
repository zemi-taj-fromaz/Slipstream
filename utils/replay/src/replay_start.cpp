#include "replay_start.h"

#include <charconv>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace utils {

namespace {

std::uint64_t ParseUnsigned(
    std::string_view text,
    std::string_view option) {
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);

    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument(
            std::string{option} + " must be an unsigned integer");
    }

    return value;
}

} // namespace

ReplayClientOptions ParseReplayClientOptions(
    int argc,
    char* const argv[]) {
    ReplayClientOptions options{};
    bool has_host = false;
    bool has_port = false;
    bool has_start = false;

    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc) {
            throw std::invalid_argument(
                "client option is missing its value");
        }

        const std::string_view option{argv[index]};
        const std::string_view value{argv[index + 1]};

        if (option == "--host") {
            options.host = value;
            has_host = true;
        } else if (option == "--transport") {
            if (value != "tcp" && value != "grpc") {
                throw std::invalid_argument(
                    "--transport must be tcp or grpc");
            }
            options.transport = value;
        } else if (option == "--port") {
            const std::uint64_t port = ParseUnsigned(value, option);
            if (port == 0 ||
                port > std::numeric_limits<std::uint16_t>::max()) {
                throw std::invalid_argument("--port must be in [1, 65535]");
            }
            options.port = static_cast<std::uint16_t>(port);
            has_port = true;
        } else if (option == "--start-at-ns") {
            options.start_at_ns = ParseUnsigned(value, option);
            has_start = true;
        } else {
            throw std::invalid_argument(
                "unknown client option: " + std::string{option});
        }
    }

    if (!has_host || !has_port || !has_start) {
        throw std::invalid_argument(
            "usage: " + std::string{argv[0]} +
            " --host <IPv4-address> --port <port> "
            "--start-at-ns <unix-nanoseconds> "
            "[--transport tcp|grpc]");
    }

    return options;
}

bool ReplayVerificationEnabled() noexcept {
    const char* value = std::getenv("SLIPSTREAM_VERIFY_REPLAY");
    return value != nullptr && std::string_view{value} == "1";
}

} // namespace utils
