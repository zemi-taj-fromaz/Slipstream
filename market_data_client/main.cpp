#include "parser.h"
#include "message_processor.h"
#include "process_rows.h"
#include "replay_start.h"
#include "slipstream.grpc.pb.h"
#include "transport/MdTcpClientTransport.h"

#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char* argv[]) {
    constexpr auto csv_path = SLIPSTREAM_CSV_PATH;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "market_data_client.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"mdclient", sinks.begin(), sinks.end()};
    logger.set_pattern("[%n] %v");

    try {
        const utils::ReplayClientOptions options =
            utils::ParseReplayClientOptions(argc, argv);
        const auto events = parse_csv(csv_path);
        logger.info("Parsed {} market event rows from {}", events.size(), csv_path);
        logger.info("Replay starts at Unix nanoseconds {}", options.start_at_ns);

        MdTcpClientTransport transport{options.host.c_str(), options.port};

        utils::ConnectionResult replay_result{};
        if (utils::ReplayVerificationEnabled()) {
            const std::string expected_path =
                std::string{SLIPSTREAM_VERIFICATION_DIR} +
                "/expected_quotes.csv";
            CanonicalFileMsgController expected_events{expected_path.c_str()};
            replay_result = ProcessRowsByTimestamp<EventType::Quote>(
                events,
                transport,
                options.start_at_ns,
                &expected_events);
        } else {
            replay_result = ProcessRowsByTimestamp<EventType::Quote>(
                events,
                transport,
                options.start_at_ns);
        }

        if (replay_result == utils::ConnectionResult::PeerDisconnected) {
            logger.info("Server closed the market-data connection");
            return 0;
        }

        logger.info("Market-data replay complete");
    } catch (const std::exception& error) {
        logger.error("Market-data client failed: {}", error.what());
        return 1;
    }

    return 0;
}
