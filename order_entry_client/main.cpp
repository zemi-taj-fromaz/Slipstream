#include "parser.h"

#include "slipstream_codec/market_data_codec.h"
#include "message_processor.h"
#include "replay_start.h"
#include "socket.h"
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
    class OEClientMsgController final : public IMsgController {
    public:
        OEClientMsgController(const char* host, std::uint16_t port) {
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
}

int main(int argc, char* argv[]) {
    constexpr auto csv_path = SLIPSTREAM_CSV_PATH;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "order_entry_client.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"order_entry_client", sinks.begin(), sinks.end()};

    try {
        const utils::ReplayClientOptions options =
            utils::ParseReplayClientOptions(argc, argv);
        const auto events = parse_csv(csv_path);
        logger.info("Parsed {} market event rows from {}", events.size(), csv_path);
        logger.info("Replay starts at Unix nanoseconds {}", options.start_at_ns);

        OEClientMsgController oe_controller{options.host.c_str(), options.port};

        if (utils::ReplayVerificationEnabled()) {
            const std::string expected_path =
                std::string{SLIPSTREAM_VERIFICATION_DIR} +
                "/expected_trades.csv";
            CanonicalFileMsgController expected_events{expected_path.c_str()};
            FanoutMsgController controller{expected_events, oe_controller};
            ProcessRowsByTimestamp<EventType::Trade>(
                events,
                controller,
                options.start_at_ns);
        } else {
            ProcessRowsByTimestamp<EventType::Trade>(
                events,
                oe_controller,
                options.start_at_ns);
        }

        logger.info("Order-entry replay complete");
    } catch (const std::exception& error) {
        logger.error("Order-entry client failed: {}", error.what());
        return 1;
    }

    return 0;
}
