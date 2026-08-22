#include "parser.h"

#include "slipstream_codec/market_data_codec.h"
#include "message_processor.h"
#include "replay_start.h"
#include "socket.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
    template <typename... Visitors>
    struct Overloaded : Visitors... {
        using Visitors::operator()...;
    };

    class OEClientMsgController final : public IMsgController {
    public:
        OEClientMsgController(
            const char* host,
            std::uint16_t port,
            spdlog::logger& logger)
            : logger_{logger} {
            constexpr int send_buffer_size = 1024 * 1024;
            socket_.SetTcpNoDelay();
            socket_.SetSendBufferSize(send_buffer_size);
            socket_.Connect(host, port);
            poll_descriptor_.fd = socket_.NativeHandle();
        }

        void Send(const MarketEvent& event) override {
            std::array<std::byte, slipstream::codec::max_market_data_frame_size> buffer{};
            const std::size_t encoded_size = slipstream::codec::EncodeMarketEvent(event, buffer);

            socket_.SendAll({buffer.data(), encoded_size});
        }

        void ProcessInboundUntil(
            std::chrono::steady_clock::time_point deadline) override {
            while (true) {
                const auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    return;
                }

                auto timeout_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        deadline - now)
                        .count();
                poll_descriptor_.events = POLLIN;
                poll_descriptor_.revents = 0;

                const int ready = ::poll(
                    &poll_descriptor_,
                    1,
                    static_cast<int>(timeout_ms));

                if (ready == -1) {
                    if (errno == EINTR) {
                        continue;
                    }

                    throw std::system_error(
                        errno,
                        std::generic_category(),
                        "OE poll() failed");
                }

                if (ready == 0) {
                    return;
                }

                if (poll_descriptor_.revents & POLLIN) {
                    ReceiveAvailable();
                }

                if (poll_descriptor_.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    throw std::runtime_error(
                        "OE socket error while waiting for server response");
                }
            }
        }

    private:
        [[nodiscard]] static std::string Symbol(const char (&symbol)[12]) {
            std::size_t length = 0;
            while (length < sizeof(symbol) && symbol[length] != '\0') {
                ++length;
            }
            return {symbol, length};
        }

        void ReceiveAvailable() {
            std::array<std::byte, 4096> buffer{};

            while (true) {
                const ::ssize_t recvd = socket_.Recv(buffer);
                if (recvd > 0) {
                    std::vector<slipstream::codec::OrderEntryClientMessage> messages;
                    const auto result = decoder_.Decode(
                        {buffer.data(), static_cast<std::size_t>(recvd)},
                        messages);

                    if (result.status == slipstream::codec::DecodeStatus::error) {
                        throw std::runtime_error(
                            "failed to decode OE server response");
                    }

                    for (const auto& message : messages) {
                        HandleMessage(message);
                    }

                    continue;
                }

                if (recvd == 0) {
                    throw std::runtime_error("server closed OE connection");
                }

                if (errno == EINTR) {
                    continue;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }

                throw std::system_error(
                    errno,
                    std::generic_category(),
                    "OE recv() failed");
            }
        }

        void HandleMessage(
            const slipstream::codec::OrderEntryClientMessage& message) {
            std::visit(
                Overloaded{
                    [this](const slipstream::codec::NewOrderMessage& value) {
                        logger_.info(
                            "NewOrder {} trade_id={} symbol={} side={} qty={} limit_px={} client_order_id={}",
                            value.status ==
                                    slipstream::codec::NewOrderStatus::accepted
                                ? "ACCEPTED"
                                : "REJECTED",
                            value.trade_id,
                            Symbol(value.symbol),
                            static_cast<char>(value.side),
                            value.qty,
                            value.limit_px,
                            value.client_order_id);
                    },
                    [this](const slipstream::codec::HeartbeatMessage& value) {
                        logger_.info("Heartbeat ts_ns={}", value.ts_ns);
                    },
                    [this](const slipstream::codec::SessionControlMessage& value) {
                        logger_.info(
                            "SessionControl state={} ts_ns={}",
                            static_cast<unsigned>(value.state),
                            value.ts_ns);
                    },
                },
                message);
        }

        utils::Socket socket_;
        pollfd poll_descriptor_{};
        slipstream::codec::ClientSideDecoder decoder_;
        spdlog::logger& logger_;
    };

    constexpr auto final_response_grace = std::chrono::milliseconds{500};
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

        OEClientMsgController oe_controller{
            options.host.c_str(),
            options.port,
            logger};

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

        oe_controller.ProcessInboundUntil(
            std::chrono::steady_clock::now() + final_response_grace);

        logger.info("Order-entry replay complete");
    } catch (const std::exception& error) {
        logger.error("Order-entry client failed: {}", error.what());
        return 1;
    }

    return 0;
}
