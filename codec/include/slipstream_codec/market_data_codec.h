#ifndef SLIPSTREAM_CODEC_MARKET_DATA_CODEC_H
#define SLIPSTREAM_CODEC_MARKET_DATA_CODEC_H

#include "market_event.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace slipstream::codec {

constexpr std::uint8_t protocol_version = 1;

constexpr std::uint8_t quote_message_type = 1;
constexpr std::uint8_t trade_message_type = 2;
constexpr std::uint8_t heartbeat_message_type = 3;
constexpr std::uint8_t session_control_message_type = 4;
constexpr std::uint8_t new_order_message_type = 10;
constexpr std::uint8_t exec_report_message_type = 11;

constexpr std::size_t frame_header_size = 4;
constexpr std::size_t quote_body_size = 44;
constexpr std::size_t trade_body_size = 41;
constexpr std::size_t heartbeat_body_size = 8;
constexpr std::size_t session_control_body_size = 9;
// The specification says 42, but its listed fields total 50 bytes.
constexpr std::size_t new_order_body_size = 50;
constexpr std::size_t exec_report_body_size = 30;
constexpr std::size_t max_market_data_frame_size =
    frame_header_size + quote_body_size;
constexpr std::size_t max_order_frame_size =
    frame_header_size + new_order_body_size;

struct HeartbeatMessage {
    std::uint64_t ts_ns{0};
};

enum class SessionState : std::uint8_t {
    open = 0,
    halt = 1,
    close = 2,
};

struct SessionControlMessage {
    std::uint64_t ts_ns{0};
    SessionState state{SessionState::open};
};

using MarketDataMessage = std::variant<
    MarketEvent,
    HeartbeatMessage,
    SessionControlMessage>;

enum class NewOrderStatus : char {
    accepted = 'A',
    rejected = 'R',
};

enum class OrderSide : char {
    buy = 'B',
    sell = 'S',
};

struct NewOrderMessage {
    std::uint64_t client_order_id{0};
    char symbol[12]{};
    NewOrderStatus status{NewOrderStatus::rejected};
    std::uint64_t ts_ns{0};
    std::int64_t trade_id{0};
    OrderSide side{OrderSide::buy};
    std::uint32_t qty{0};
    std::int64_t limit_px{0};
};

enum class ExecStatus : std::uint8_t {
    ack = 0,
    fill = 1,
    partial = 2,
    reject = 3,
};

enum class RejectReason : std::uint8_t {
    none = 0,
    risk = 1,
    price = 2,
    size = 3,
    throttle = 4,
};

struct ExecReportMessage {
    std::uint64_t client_order_id{0};
    std::uint64_t ts_ns{0};
    ExecStatus status{ExecStatus::ack};
    std::uint32_t filled_qty{0};
    std::int64_t avg_px{0};
    RejectReason reason_code{RejectReason::none};
};

using OrderMessage = std::variant<NewOrderMessage, ExecReportMessage>;

enum class DecodeStatus {
    message_ready,
    need_more_data,
    error,
};

struct DecodeResult {
    DecodeStatus status{DecodeStatus::need_more_data};
    std::size_t bytes_consumed{0};
};

struct StreamDecodeResult {
    DecodeStatus status{DecodeStatus::need_more_data};
    std::size_t messages_decoded{0};
};

[[nodiscard]] std::size_t EncodeMarketEvent(
    const MarketEvent& event,
    std::span<std::byte> output);

[[nodiscard]] DecodeResult DecodeMarketEvent(
    std::span<const std::byte> input,
    MarketEvent& output);

[[nodiscard]] std::size_t EncodeHeartbeat(
    const HeartbeatMessage& message,
    std::span<std::byte> output);

[[nodiscard]] DecodeResult DecodeHeartbeat(
    std::span<const std::byte> input,
    HeartbeatMessage& output);

[[nodiscard]] std::size_t EncodeSessionControl(
    const SessionControlMessage& message,
    std::span<std::byte> output);

[[nodiscard]] DecodeResult DecodeSessionControl(
    std::span<const std::byte> input,
    SessionControlMessage& output);

[[nodiscard]] DecodeResult DecodeMarketDataMessage(
    std::span<const std::byte> input,
    MarketDataMessage& output);

class MarketDataStreamDecoder {
public:
    [[nodiscard]] StreamDecodeResult Decode(
        std::span<const std::byte> input,
        std::vector<MarketDataMessage>& output);

    [[nodiscard]] std::size_t BufferedBytes() const noexcept;

private:
    std::vector<std::byte> pending_;
};

[[nodiscard]] std::size_t EncodeNewOrder(
    const NewOrderMessage& message,
    std::span<std::byte> output);

[[nodiscard]] DecodeResult DecodeNewOrder(
    std::span<const std::byte> input,
    NewOrderMessage& output);

[[nodiscard]] std::size_t EncodeExecReport(
    const ExecReportMessage& message,
    std::span<std::byte> output);

[[nodiscard]] DecodeResult DecodeExecReport(
    std::span<const std::byte> input,
    ExecReportMessage& output);

[[nodiscard]] DecodeResult DecodeOrderMessage(
    std::span<const std::byte> input,
    OrderMessage& output);

class OrderStreamDecoder {
public:
    [[nodiscard]] StreamDecodeResult Consume(
        std::span<const std::byte> input,
        std::vector<OrderMessage>& output);

    [[nodiscard]] std::size_t BufferedBytes() const noexcept;

private:
    std::vector<std::byte> pending_;
};

} // namespace slipstream::codec

#endif // SLIPSTREAM_CODEC_MARKET_DATA_CODEC_H
