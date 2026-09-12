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
    char* const argv[],
    const unsigned default_cpu) {
    ReplayClientOptions options{};
    options.cpu = default_cpu;
    bool has_host = false;
    bool has_port = false;
    bool has_start = false;

    for (int index = 1; index < argc;) {
        const std::string_view option{argv[index]};
        if (option == "--benchmark") {
            options.benchmark = true;
            ++index;
            continue;
        }

        if (index + 1 >= argc) {
            throw std::invalid_argument(
                "client option is missing its value");
        }

        const std::string_view value{argv[index + 1]};

        if (option == "--host") {
            options.host = value;
            has_host = true;
        } else if (option == "--transport") {
            if (value != "tcp" &&
                value != "grpc" &&
                value != "udp-multicast") {
                throw std::invalid_argument(
                    "--transport must be tcp, grpc, or udp-multicast");
            }
            options.transport = value;
        } else if (option == "--execution-mode") {
            if (value != "engine_wait" && value != "engine_spin" && value != "engine_probe") {
                throw std::invalid_argument(
                    "--execution-mode must be engine_wait, engine_spin, or engine_probe");
            }
            options.execution_mode = value;
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
        } else if (option == "--cpu") {
            const std::uint64_t cpu = ParseUnsigned(value, option);
            if (cpu > std::numeric_limits<unsigned>::max()) {
                throw std::invalid_argument(
                    "--cpu exceeds the supported range");
            }
            options.cpu = static_cast<unsigned>(cpu);
        } else if (option == "--md-a-group") {
            options.md_a_group = value;
        } else if (option == "--md-a-port") {
            const std::uint64_t port = ParseUnsigned(value, option);
            if (port == 0 ||
                port > std::numeric_limits<std::uint16_t>::max()) {
                throw std::invalid_argument(
                    "--md-a-port must be in [1, 65535]");
            }
            options.md_a_port = static_cast<std::uint16_t>(port);
        } else if (option == "--md-b-group") {
            options.md_b_group = value;
        } else if (option == "--md-b-port") {
            const std::uint64_t port = ParseUnsigned(value, option);
            if (port == 0 ||
                port > std::numeric_limits<std::uint16_t>::max()) {
                throw std::invalid_argument(
                    "--md-b-port must be in [1, 65535]");
            }
            options.md_b_port = static_cast<std::uint16_t>(port);
        } else if (option == "--md-multicast-interface") {
            options.md_multicast_interface = value;
        } else {
            throw std::invalid_argument(
                "unknown client option: " + std::string{option});
        }

        index += 2;
    }

    if (!has_host || !has_port || !has_start) {
        throw std::invalid_argument(
            "usage: " + std::string{argv[0]} +
            " --host <IPv4-address> --port <port> "
            "--start-at-ns <unix-nanoseconds> "
            "[--benchmark] "
            "[--execution-mode engine_wait|engine_spin|engine_probe] "
            "[--cpu <logical-cpu>] "
            "[--transport tcp|grpc|udp-multicast] "
            "[--md-a-group <IPv4-multicast-address>] "
            "[--md-a-port <port>] "
            "[--md-b-group <IPv4-multicast-address>] "
            "[--md-b-port <port>] "
            "[--md-multicast-interface <IPv4-address>]");
    }

    return options;
}

bool ReplayVerificationEnabled() noexcept {
    const char* value = std::getenv("SLIPSTREAM_VERIFY_REPLAY");
    return value != nullptr && std::string_view{value} == "1";
}

} // namespace utils
