#include "parser.h"
#include "message_processor.h"
#include "replay_start.h"
#include "socket.h"

#include "slipstream_codec/market_data_codec.h"

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

class CodecProcessMsg final : public IProcessMsgClass {
public:
    explicit CodecProcessMsg(spdlog::logger& logger)
        : logger_{logger} {
    }

    void Sink(const MarketEvent& event) override {
        std::array<std::byte, slipstream::codec::max_market_data_frame_size> buffer{};
        const std::size_t encoded_size =
            slipstream::codec::EncodeMarketEvent(event, buffer);

        logger_.info("Encoded market event into {} bytes", encoded_size);
    }

private:
    spdlog::logger& logger_;
};

class NetworkProcessMsg final : public IProcessMsgClass {
public:
    NetworkProcessMsg(const char* host, std::uint16_t port) {
        constexpr int send_buffer_size = 1024 * 1024;
        socket_.SetTcpNoDelay();
        socket_.SetSendBufferSize(send_buffer_size);
        socket_.Connect(host, port);
    }

    void Sink(const MarketEvent& event) override {
        std::array<std::byte, slipstream::codec::max_market_data_frame_size> buffer{};
        const std::size_t encoded_size = slipstream::codec::EncodeMarketEvent(event, buffer);

        socket_.SendAll({buffer.data(), encoded_size});

        //logger_.info("Encoded market event into {} bytes", encoded_size);
    }

private:
    utils::Socket socket_;
};

} // namespace

int main(int argc, char* argv[]) {
    constexpr auto csv_path = SLIPSTREAM_CSV_PATH;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "market_data_client.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"market_data_client", sinks.begin(), sinks.end()};

    try {
        const utils::ReplayClientOptions options =
            utils::ParseReplayClientOptions(argc, argv);
        const auto events = parse_csv(csv_path);
        logger.info("Parsed {} market event rows from {}", events.size(), csv_path);
        logger.info("Replay starts at Unix nanoseconds {}", options.start_at_ns);

        NetworkProcessMsg net_processor{options.host.c_str(), options.port};

        if (utils::ReplayVerificationEnabled()) {
            const std::string expected_path =
                std::string{SLIPSTREAM_VERIFICATION_DIR} +
                "/expected_quotes.csv";
            CanonicalFileProcessMsg expected_events{expected_path.c_str()};
            FanoutProcessMsg processor{expected_events, net_processor};
            ProcessRowsByTimestamp<EventType::Quote>(
                events,
                processor,
                options.start_at_ns);
        } else {
            ProcessRowsByTimestamp<EventType::Quote>(
                events,
                net_processor,
                options.start_at_ns);
        }

        logger.info("Market-data replay complete");
    } catch (const std::exception& error) {
        logger.error("Market-data client failed: {}", error.what());
        return 1;
    }

    return 0;
}
