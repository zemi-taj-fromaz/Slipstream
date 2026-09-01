//
// Created by babodev on 15.08.2026..
//

#include "SlipstreamConfig.h"
#include "Queues.h"

#include <exception>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "NetworkManager.h"
#include "Engine.h"

#include <thread>

namespace {

template <typename Number>
Number ParseNumber(
    std::string_view text,
    std::string_view option) {
    Number value{};

    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);

    if (error != std::errc{} ||
        end != text.data() + text.size()) {
        throw std::invalid_argument(
            std::string{option} +
            " has an invalid numeric value");
    }

    return value;
}

} // namespace

SlipstreamConfig ParseSlipstreamConfig(int argc, char* argv[]) {

    SlipstreamConfig config;

    for (int index = 1; index < argc; ++index) {
        const std::string_view option{argv[index]};

        const auto NextValue = [&]() -> std::string_view {
            if (index + 1 >= argc) {
                throw std::invalid_argument(
                    std::string{option} +
                    " is missing its value");
            }

            return argv[++index];
        };

        if (option == "--symbol") {
            config.symbol = NextValue();

        } else if (option == "--max-quantity") {
            config.max_quantity =
                ParseNumber<std::uint32_t>(
                    NextValue(),
                    option);

        } else if (option == "--participation-cap") {
            config.participation_cap =
                ParseNumber<double>(
                    NextValue(),
                    option);

        } else if (option == "--vwap-window-ms") {
            config.vwap_window_ms =
                ParseNumber<std::uint32_t>(
                    NextValue(),
                    option);

        } else if (option == "--band-bps") {
            config.band_bps =
                ParseNumber<double>(
                    NextValue(),
                    option);

        } else if (option == "--md-host") {
            config.md_host = NextValue();

        } else if (option == "--md-port") {
            config.md_port =
                ParseNumber<std::uint16_t>(
                    NextValue(),
                    option);

        } else if (option == "--oe-host") {
            config.oe_host = NextValue();

        } else if (option == "--oe-port") {
            config.oe_port =
                ParseNumber<std::uint16_t>(
                    NextValue(),
                    option);

        } else if (option == "--transport") {
            config.transport = NextValue();

        } else {
            throw std::invalid_argument(
                "unknown argument: " +
                std::string{option});
        }
    }

    if (config.md_port == 0 || config.oe_port == 0) {
        throw std::invalid_argument(
            "MD and OE ports must be greater than zero");
    }

    return config;
}

int main(int argc, char* argv[]) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "slipstream_server.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"slipstream", sinks.begin(), sinks.end()};
    logger.set_pattern("[%n] %v");

    try {
        const SlipstreamConfig slipstream_config = ParseSlipstreamConfig(argc, argv);
        slipstream::MarketEventQueue ingress;
        slipstream::OrderEntryQueue egress;
        std::atomic<std::uint64_t> ingress_generation{0};

        slipstream::NetworkManager network_manager{
            slipstream_config,
            ingress,
            egress,
            ingress_generation};
        Engine engine{
            slipstream_config,
            ingress,
            egress,
            ingress_generation,
            [&network_manager] {
                network_manager.SignalEvent();
            }};

        std::exception_ptr network_error;
        std::exception_ptr engine_error;

        std::jthread network_thread{[&] {
            try {
                network_manager.Run();
            } catch (...) {
                network_error = std::current_exception();
            }
        }};

        std::jthread engine_thread{[&] {
            try {
                engine.Run();
            } catch (...) {
                engine_error = std::current_exception();
            }
        }};

        network_thread.join();
        engine.Stop();
        engine_thread.join();

        if (network_error) {
            std::rethrow_exception(network_error);
        }
        if (engine_error) {
            std::rethrow_exception(engine_error);
        }

        ExecutionReport execution_report = engine.GetExecutionReport();
        execution_report.tick_to_order =
            network_manager.GetTickToOrderStatistics();

        const std::string report = FormatExecutionReport(
            execution_report,
            slipstream_config);

        std::ofstream report_file{
            SLIPSTREAM_EXECUTION_REPORT_PATH,
            std::ios::trunc};
        if (!report_file) {
            throw std::runtime_error(
                "failed to open execution report file");
        }

        report_file << report;
        if (!report_file) {
            throw std::runtime_error(
                "failed to write execution report file");
        }

        std::cout << report << std::flush;
        logger.info(
            "Execution report written to {}",
            SLIPSTREAM_EXECUTION_REPORT_PATH);

        return 0;
    } catch (const std::exception& error) {
        logger.error("Slipstream failed: {}", error.what());
        return 1;
    }
}
