//
// Created by babodev on 15.08.2026..
//

#include "SlipstreamConfig.h"
#include "Queues.h"
#include "thread_config.h"

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
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "transport/TcpPollServerTransport.h"
#include "transport/GrpcServerTransport.h"
#include "transport/UdpMulticastServerTransport.h"
#include "Engine.h"
#include "slipstream.grpc.pb.h"

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

        } else if (option == "--md-a-group") {
            config.md_a_group = NextValue();

        } else if (option == "--md-a-port") {
            config.md_a_port =
                ParseNumber<std::uint16_t>(
                    NextValue(),
                    option);

        } else if (option == "--md-b-group") {
            config.md_b_group = NextValue();

        } else if (option == "--md-b-port") {
            config.md_b_port =
                ParseNumber<std::uint16_t>(
                    NextValue(),
                    option);

        } else if (option == "--md-multicast-interface") {
            config.md_multicast_interface = NextValue();

        } else if (option == "--oe-host") {
            config.oe_host = NextValue();

        } else if (option == "--oe-port") {
            config.oe_port =
                ParseNumber<std::uint16_t>(
                    NextValue(),
                    option);

        } else if (option == "--transport") {
            config.transport = NextValue();

        } else if (option == "--execution-mode") {
            config.execution_mode = ParseExecutionMode(NextValue());

        } else if (option == "--benchmark") {
            config.benchmark = true;

        } else if (option == "--main-cpu") {
            config.main_cpu =
                ParseNumber<unsigned>(NextValue(), option);

        } else if (option == "--network-cpu") {
            config.network_cpu =
                ParseNumber<unsigned>(NextValue(), option);

        } else if (option == "--engine-cpu") {
            config.engine_cpu =
                ParseNumber<unsigned>(NextValue(), option);

        } else {
            throw std::invalid_argument(
                "unknown argument: " +
                std::string{option});
        }
    }

    if (config.md_port == 0 ||
        config.md_a_port == 0 ||
        config.md_b_port == 0 ||
        config.oe_port == 0) {
        throw std::invalid_argument(
            "MD, multicast, and OE ports must be greater than zero");
    }

    return config;
}

int main(int argc, char* argv[]) {
    const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    const auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "slipstream_server.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"slipstream", sinks.begin(), sinks.end()};
    logger.set_pattern("[%n] %v");

    try {
        const SlipstreamConfig slipstream_config = ParseSlipstreamConfig(argc, argv);
        if (slipstream_config.benchmark) {
            logger.set_level(spdlog::level::off);
            spdlog::set_level(spdlog::level::off);
        }
        utils::ConfigureCurrentThread(
            "slip-main",
            slipstream_config.main_cpu);
        slipstream::MarketEventQueue ingress;
        slipstream::OrderEntryQueue egress;
        std::atomic<std::uint64_t> ingress_generation{0};

        std::unique_ptr<slipstream::IServerTransport> server_transport;

        if (slipstream_config.transport == "grpc") {
            server_transport =
                std::make_unique<slipstream::GrpcServerTransport>(
                    slipstream_config,
                    ingress,
                    egress,
                    ingress_generation);
        } else if (slipstream_config.transport == "tcp") {
            server_transport =
                std::make_unique<slipstream::TcpPollServerTransport>(
                    slipstream_config,
                    ingress,
                    egress,
                    ingress_generation);
        } else if (slipstream_config.transport == "udp-multicast") {
            server_transport =
                std::make_unique<slipstream::UdpMulticastServerTransport>(
                    slipstream_config,
                    ingress,
                    egress,
                    ingress_generation);
        } else {
            throw std::invalid_argument(
                "--transport must be tcp, grpc, or udp-multicast");
        }

        Engine engine{
            slipstream_config,
            ingress,
            egress,
            ingress_generation,
            [&server_transport] {
                server_transport->NotifyOutboundReady();
            }};

        std::exception_ptr network_error;
        std::exception_ptr engine_error;

        std::jthread network_thread{[&] {
            try {
                utils::ConfigureCurrentThread(
                    "slip-network",
                    slipstream_config.network_cpu);
                server_transport->Run();
            } catch (...) {
                network_error = std::current_exception();
            }
        }};

        std::jthread engine_thread{[&] {
            try {
                utils::ConfigureCurrentThread(
                    "slip-engine",
                    slipstream_config.engine_cpu);
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
            server_transport->GetTickToOrderStatistics();

        const std::string report = FormatExecutionReport(
            execution_report,
            slipstream_config);

        const std::string_view order_transport =
            slipstream_config.transport == "grpc"
                ? "grpc"
                : "tcp";
        const std::string benchmark_output_dir{
            SLIPSTREAM_BENCHMARK_OUTPUT_DIR};
        const std::string output_suffix = std::string{order_transport} + "_" +
            std::string{ExecutionModeName(slipstream_config.execution_mode)};
        const std::string report_path =
            benchmark_output_dir +
            "/execution_report_" +
            output_suffix + ".log";
        std::ofstream report_file{
            report_path,
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

        if (slipstream_config.benchmark) {
            const std::string raw_tto_path =
                benchmark_output_dir +
                "/tick_to_order_raw_" +
                output_suffix + ".csv";
            std::ofstream raw_tto_file{raw_tto_path, std::ios::trunc};
            if (!raw_tto_file) {
                throw std::runtime_error(
                    "failed to open raw tick-to-order CSV");
            }

            server_transport->GetTickToOrderHistogram().WriteRawCsv(
                raw_tto_file, order_transport, slipstream_config.execution_mode);
            if (!raw_tto_file) {
                throw std::runtime_error(
                    "failed to write raw tick-to-order CSV");
            }
        }

        std::cout << report << std::flush;
        logger.info(
            "Execution report written to {}",
            report_path);
        return 0;
    } catch (const std::exception& error) {
        logger.error("Slipstream failed: {}", error.what());
        return 1;
    }
}
