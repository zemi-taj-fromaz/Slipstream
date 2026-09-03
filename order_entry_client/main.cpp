#include "parser.h"

#include "message_processor.h"
#include "process_rows.h"
#include "replay_start.h"
#include "slipstream.grpc.pb.h"
#include "transport/OeGrpcClientTransport.h"
#include "transport/OeTcpClientTransport.h"
#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
    constexpr auto final_response_grace = std::chrono::milliseconds{500};
}

int main(int argc, char* argv[]) {
    constexpr auto csv_path = SLIPSTREAM_CSV_PATH;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "order_entry_client.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"oeclient", sinks.begin(), sinks.end()};
    logger.set_pattern("[%n] %v");

    try {
        const utils::ReplayClientOptions options =
            utils::ParseReplayClientOptions(argc, argv);
        const auto events = parse_csv(csv_path);
        logger.info("Parsed {} market event rows from {}", events.size(), csv_path);
        logger.info("Replay starts at Unix nanoseconds {}", options.start_at_ns);

        std::unique_ptr<IClientTransport> transport;
        if (options.transport == "grpc") {
            transport = std::make_unique<OeGrpcClientTransport>(
                options.host,
                options.port,
                logger);
        } else {
            transport = std::make_unique<OeTcpClientTransport>(
                options.host.c_str(),
                options.port,
                logger);
        }

        utils::ConnectionResult replay_result{};
        if (utils::ReplayVerificationEnabled()) {
            const std::string expected_path =
                std::string{SLIPSTREAM_VERIFICATION_DIR} +
                "/expected_trades.csv";
            CanonicalFileMsgController expected_events{expected_path.c_str()};
            replay_result = ProcessRowsByTimestamp<EventType::Trade>(
                events,
                *transport,
                options.start_at_ns,
                &expected_events);
        } else {
            replay_result = ProcessRowsByTimestamp<EventType::Trade>(
                events,
                *transport,
                options.start_at_ns);
        }

        // The server disconnected while the order-entry replay was running.
        if (replay_result == utils::ConnectionResult::PeerDisconnected) {
            transport->Finish();
            logger.info("Server closed the order-entry connection");
            return 0;
        }

        const utils::ConnectionResult final_result =
            transport->ProcessInboundUntil(
            std::chrono::steady_clock::now() + final_response_grace);

        // The server disconnected while the client awaited final responses.
        if (final_result == utils::ConnectionResult::PeerDisconnected) {
            transport->Finish();
            logger.info("Server closed the order-entry connection");
            return 0;
        }

        // The replay ended naturally; close the transport connection.
        if (transport->Finish() ==
            utils::ConnectionResult::PeerDisconnected) {
            logger.info("Server closed the order-entry connection");
            return 0;
        }

        logger.info("Order-entry replay complete");
    } catch (const std::exception& error) {
        logger.error("Order-entry client failed: {}", error.what());
        return 1;
    }

    return 0;
}
