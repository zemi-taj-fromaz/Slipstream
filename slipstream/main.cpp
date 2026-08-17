//
// Created by babodev on 15.08.2026..
//
#include <exception>
#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "NetworkManager.h"

namespace {

struct NetworkOptions {
    std::string md_host{"127.0.0.1"};
    std::uint16_t md_port{9001};
    std::string oe_host{"127.0.0.1"};
    std::uint16_t oe_port{9002};
};

std::uint16_t ParsePort(std::string_view text, std::string_view option) {
    std::uint32_t value{};
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);

    if (error != std::errc{} ||
        end != text.data() + text.size() ||
        value == 0 ||
        value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument(
            std::string{option} + " must be in [1, 65535]");
    }

    return static_cast<std::uint16_t>(value);
}

NetworkOptions ParseNetworkOptions(int argc, char* argv[]) {
    NetworkOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view option{argv[index]};
        if (option != "--md-host" &&
            option != "--md-port" &&
            option != "--oe-host" &&
            option != "--oe-port") {
            continue;
        }

        if (index + 1 >= argc) {
            throw std::invalid_argument(
                std::string{option} + " is missing its value");
        }

        const std::string_view value{argv[++index]};
        if (option == "--md-host") {
            options.md_host = value;
        } else if (option == "--md-port") {
            options.md_port = ParsePort(value, option);
        } else if (option == "--oe-host") {
            options.oe_host = value;
        } else {
            options.oe_port = ParsePort(value, option);
        }
    }

    return options;
}

} // namespace

int main(int argc, char* argv[]) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "slipstream_server.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"slipstream", sinks.begin(), sinks.end()};

    try {
        const NetworkOptions options = ParseNetworkOptions(argc, argv);
        logger.info(
            "Listening for MD on {}:{} and OE on {}:{}",
            options.md_host,
            options.md_port,
            options.oe_host,
            options.oe_port);

        slipstream::NetworkManager network_manager{
            options.md_host,
            options.md_port,
            options.oe_host,
            options.oe_port};
        network_manager.Process();
        return 0;
    } catch (const std::exception& error) {
        logger.error("Slipstream failed: {}", error.what());
        return 1;
    }
}
