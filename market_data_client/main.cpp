#include "parser.h"
#include "message_processor.h"
#include "socket.h"

#include "slipstream_codec/market_data_codec.h"

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "../utils/message_processor/include/message_processor.h"

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
    NetworkProcessMsg(std::uint16_t port) {
        constexpr int send_buffer_size = 1024 * 1024;
        socket_.SetTcpNoDelay();
        socket_.SetSendBufferSize(send_buffer_size);
        socket_.Connect("127.0.0.1", port);
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

int main() {
    constexpr auto csv_path = SLIPSTREAM_CSV_PATH;
    constexpr auto event_log_path = SLIPSTREAM_EVENT_LOG_PATH;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "market_data_client.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"market_data_client", sinks.begin(), sinks.end()};

    try {
        const auto events = parse_csv(csv_path);
        logger.info("Parsed {} market event rows from {}", events.size(), csv_path);

        logger.info(
            "Starting real-time replay; output file: {}",
            event_log_path);

        NetworkProcessMsg net_processor{9001};
        ProcessRowsByTimestamp<EventType::Quote>(events, net_processor);

        logger.info("Replay complete; wrote {} market events to {}", events.size(), event_log_path);
    } catch (const std::exception& error) {
        logger.error("Failed to parse {}: {}", csv_path, error.what());
        return 1;
    }

    return 0;
}
