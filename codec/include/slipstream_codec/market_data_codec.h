#ifndef SLIPSTREAM_CODEC_MARKET_DATA_CODEC_H
#define SLIPSTREAM_CODEC_MARKET_DATA_CODEC_H

#include "market_event.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace slipstream::codec {

constexpr std::uint8_t protocol_version = 1;

constexpr std::uint8_t quote_message_type = 1;
constexpr std::uint8_t trade_message_type = 2;
constexpr std::uint8_t heartbeat_message_type = 3;
constexpr std::uint8_t session_control_message_type = 4;

constexpr std::size_t frame_header_size = 4;
constexpr std::size_t quote_body_size = 44;
constexpr std::size_t trade_body_size = 41;
constexpr std::size_t max_market_data_frame_size =
    frame_header_size + quote_body_size;

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

class MarketDataStreamDecoder {
public:
    [[nodiscard]] StreamDecodeResult Consume(
        std::span<const std::byte> input,
        std::vector<MarketEvent>& output);

    [[nodiscard]] std::size_t BufferedBytes() const noexcept;

private:
    std::vector<std::byte> pending_;
};

} // namespace slipstream::codec

#endif // SLIPSTREAM_CODEC_MARKET_DATA_CODEC_H
