#include "slipstream_codec/market_data_codec.h"

#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <variant>
#include <vector>

namespace slipstream::codec {

namespace {

template <typename Integer>
Integer readUnsignedLittleEndian(std::span<const std::byte> bytes) {
    Integer value{0};
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        value |= static_cast<Integer>(std::to_integer<unsigned int>(bytes[index]))
                 << (index * 8);
    }
    return value;
}

std::uint16_t readUint16LittleEndian(std::span<const std::byte> bytes) {
    return readUnsignedLittleEndian<std::uint16_t>(bytes);
}

std::uint32_t readUint32LittleEndian(std::span<const std::byte> bytes) {
    return readUnsignedLittleEndian<std::uint32_t>(bytes);
}

std::uint64_t readUint64LittleEndian(std::span<const std::byte> bytes) {
    return readUnsignedLittleEndian<std::uint64_t>(bytes);
}

std::int64_t readInt64LittleEndian(std::span<const std::byte> bytes) {
    return std::bit_cast<std::int64_t>(readUint64LittleEndian(bytes));
}

void writeUint16LittleEndian(std::span<std::byte> bytes, uint16_t value) {
    bytes[0] = static_cast<std::byte>(value & 0xFF);
    bytes[1] = static_cast<std::byte>((value >> 8) & 0xFF);
}

void writeUint32LittleEndian(std::span<std::byte> bytes, uint32_t value) {
    bytes[0] = static_cast<std::byte>(value & 0xFF);
    bytes[1] = static_cast<std::byte>((value >> 8) & 0xFF);
    bytes[2] = static_cast<std::byte>((value >> 16) & 0xFF);
    bytes[3] = static_cast<std::byte>((value >> 24) & 0xFF);
}

void writeUint64LittleEndian(std::span<std::byte> bytes, uint64_t value) {
    bytes[0] = static_cast<std::byte>(value & 0xFF);
    bytes[1] = static_cast<std::byte>((value >> 8) & 0xFF);
    bytes[2] = static_cast<std::byte>((value >> 16) & 0xFF);
    bytes[3] = static_cast<std::byte>((value >> 24) & 0xFF);
    bytes[4] = static_cast<std::byte>((value >> 32) & 0xFF);
    bytes[5] = static_cast<std::byte>((value >> 40) & 0xFF);
    bytes[6] = static_cast<std::byte>((value >> 48) & 0xFF);
    bytes[7] = static_cast<std::byte>((value >> 56) & 0xFF);
}

void writeInt64LittleEndian(std::span<std::byte> bytes, int64_t value) {
    writeUint64LittleEndian(bytes, std::bit_cast<std::uint64_t>(value));
}

void encodeHeader(std::span<std::byte> bytes, std::uint8_t msgType, std::uint16_t body_len) {
    writeUint16LittleEndian(bytes.subspan(0, 2), body_len);
    bytes[2] = static_cast<std::byte>(msgType);
    bytes[3] = static_cast<std::byte>(slipstream::codec::protocol_version);
}

bool hasExpectedHeader(
    std::span<const std::byte> input,
    std::uint8_t expected_message_type,
    std::size_t expected_body_size) {
    return input.size() >= frame_header_size &&
           readUint16LittleEndian(input.first(2)) == expected_body_size &&
           std::to_integer<std::uint8_t>(input[2]) == expected_message_type &&
           std::to_integer<std::uint8_t>(input[3]) == protocol_version;
}

std::size_t encodeQuoteBody(std::span<std::byte> bytes, const MarketEvent& event) {
    std::size_t offset{0uz};
    const Quote& quote = std::get<Quote>(event.payload);


    std::memcpy(bytes.data() + offset, event.symbol, 12);
    offset += 12uz;

    writeUint64LittleEndian(bytes.subspan(offset, 8), event.ts);
    offset += 8uz;

    writeUint32LittleEndian(bytes.subspan(offset, 4), quote.bid_qty);
    offset += 4uz;

    writeInt64LittleEndian(bytes.subspan(offset, 8), quote.bid_price);
    offset += 8uz;

    writeUint32LittleEndian(bytes.subspan(offset, 4), quote.ask_qty);
    offset += 4uz;

    writeInt64LittleEndian(bytes.subspan(offset, 8), quote.ask_price);
    offset += 8uz;
    return offset;
}

std::size_t decodeQuoteBody(std::span<const std::byte> bytes, MarketEvent& out) {
    std::size_t offset{0uz};
    out.payload = Quote{};
    Quote& quote = std::get<Quote>(out.payload);

    std::memcpy(out.symbol, bytes.data() + offset, 12);
    offset += 12uz;

    out.ts = readUint64LittleEndian(bytes.subspan(offset, 8));
    offset += 8uz;

    quote.bid_qty = readUint32LittleEndian(bytes.subspan(offset, 4));
    offset += 4uz;

    quote.bid_price = readInt64LittleEndian(bytes.subspan(offset, 8));
    offset += 8uz;

    quote.ask_qty = readUint32LittleEndian(bytes.subspan(offset, 4));
    offset += 4uz;

    quote.ask_price = readInt64LittleEndian(bytes.subspan(offset, 8));
    offset += 8uz;
    return offset;
}

std::size_t encodeTradeBody(std::span<std::byte> bytes, const MarketEvent& event) {
    std::size_t offset{0uz};
    const Trade& trade = std::get<Trade>(event.payload);


    std::memcpy(bytes.data() + offset, event.symbol, 12);
    offset += 12uz;

    writeUint64LittleEndian(bytes.subspan(offset, 8), event.ts);
    offset += 8uz;

    writeUint32LittleEndian(bytes.subspan(offset, 4), trade.qty);
    offset += 4uz;

    writeInt64LittleEndian(bytes.subspan(offset, 8), trade.price);
    offset += 8uz;

    bytes[offset] = static_cast<std::byte>(trade.aggressor);
    offset += 1uz;

    writeInt64LittleEndian(bytes.subspan(offset, 8), trade.id);
    offset += 8uz;
    return offset;
}


std::size_t decodeTradeBody(std::span<const std::byte> bytes, MarketEvent& out) {
    std::size_t offset{0uz};
    out.payload = Trade{};
    Trade& trade = std::get<Trade>(out.payload);

    std::memcpy(out.symbol, bytes.data() + offset, 12);
    offset += 12uz;

    out.ts = readUint64LittleEndian(bytes.subspan(offset, 8));
    offset += 8uz;

    trade.qty = readUint32LittleEndian(bytes.subspan(offset, 4));
    offset += 4uz;

    trade.price = readInt64LittleEndian(bytes.subspan(offset, 8));
    offset += 8uz;

    trade.aggressor = static_cast<char>(std::to_integer<unsigned char>(bytes[offset]));
    offset += 1uz;

    trade.id = readInt64LittleEndian(bytes.subspan(offset, 8));
    offset += 8uz;
    return offset;
}

bool isValid(NewOrderStatus status) {
    return status == NewOrderStatus::accepted ||
           status == NewOrderStatus::rejected;
}

bool isValid(OrderSide side) {
    return side == OrderSide::buy || side == OrderSide::sell;
}

}


std::size_t EncodeMarketEvent(const MarketEvent& event, std::span<std::byte> output) {
    if (std::holds_alternative<Quote>(event.payload)) {
        constexpr std::size_t frame_size = frame_header_size + quote_body_size;
        if (output.size() < frame_size) {
            throw std::runtime_error("output buffer too small for quote frame");
        }

        encodeHeader(output.first(frame_header_size), quote_message_type, quote_body_size);
        encodeQuoteBody(output.subspan(frame_header_size, quote_body_size), event);
        return frame_size;
    }

    if (std::holds_alternative<Trade>(event.payload)) {
        constexpr std::size_t frame_size = frame_header_size + trade_body_size;
        if (output.size() < frame_size) {
            throw std::runtime_error("output buffer too small for trade frame");
        }

        encodeHeader(output.first(frame_header_size), trade_message_type, trade_body_size);
        encodeTradeBody(output.subspan(frame_header_size, trade_body_size), event);
        return frame_size;
    }

    throw std::logic_error("unknown MarketEvent payload type");
}

DecodeResult DecodeMarketEvent(std::span<const std::byte> input, MarketEvent& output) {
    if (input.size() < frame_header_size) {
        return {DecodeStatus::need_more_data, 0};
    }

    const std::uint16_t body_len = readUint16LittleEndian(input.subspan(0, 2));
    const std::uint8_t msg_type = std::to_integer<std::uint8_t>(input[2]);
    const std::uint8_t version = std::to_integer<std::uint8_t>(input[3]);

    if (version != protocol_version) {
        return {DecodeStatus::error, 0};
    }

    const std::size_t frame_size = frame_header_size + body_len;
    if (input.size() < frame_size) {
        return {DecodeStatus::need_more_data, 0};
    }

    const auto body = input.subspan(frame_header_size, body_len);

    if (msg_type == quote_message_type) {
        if (body_len != quote_body_size) {
            return {DecodeStatus::error, 0};
        }

        decodeQuoteBody(body, output);
        return {DecodeStatus::message_ready, frame_size};
    }

    if (msg_type == trade_message_type) {
        if (body_len != trade_body_size) {
            return {DecodeStatus::error, 0};
        }

        decodeTradeBody(body, output);
        return {DecodeStatus::message_ready, frame_size};
    }

    return {DecodeStatus::error, 0};
}

std::size_t EncodeMulticastMarketData(
    const MulticastMarketDataDatagram& datagram,
    std::span<std::byte> output) {
    if (output.size() < multicast_header_size) {
        throw std::runtime_error("output buffer too small for multicast header");
    }

    writeUint64LittleEndian(
        output.first(multicast_header_size),
        datagram.header.sequence);

    const std::size_t frame_size = EncodeMarketEvent(
        datagram.event,
        output.subspan(multicast_header_size));
    return multicast_header_size + frame_size;
}

DecodeResult DecodeMulticastMarketData(
    std::span<const std::byte> input,
    MulticastMarketDataDatagram& output) {
    if (input.size() < multicast_header_size) {
        return {DecodeStatus::error, 0};
    }

    const DecodeResult frame_result = DecodeMarketEvent(
        input.subspan(multicast_header_size),
        output.event);
    if (frame_result.status != DecodeStatus::message_ready) {
        return {DecodeStatus::error, 0};
    }

    const std::size_t datagram_size =
        multicast_header_size + frame_result.bytes_consumed;
    if (input.size() != datagram_size) {
        return {DecodeStatus::error, 0};
    }

    output.header.sequence = readUint64LittleEndian(
        input.first(multicast_header_size));
    return {DecodeStatus::message_ready, datagram_size};
}

std::size_t EncodeHeartbeat(
    const HeartbeatMessage& message,
    std::span<std::byte> output) {
    constexpr std::size_t frame_size = frame_header_size + heartbeat_body_size;
    if (output.size() < frame_size) {
        throw std::runtime_error("output buffer too small for heartbeat frame");
    }

    encodeHeader(output.first(frame_header_size), heartbeat_message_type, heartbeat_body_size);
    writeUint64LittleEndian(
        output.subspan(frame_header_size, heartbeat_body_size),
        message.ts_ns);
    return frame_size;
}

DecodeResult DecodeHeartbeat(
    std::span<const std::byte> input,
    HeartbeatMessage& output) {
    constexpr std::size_t frame_size = frame_header_size + heartbeat_body_size;
    if (input.size() < frame_header_size) {
        return {DecodeStatus::need_more_data, 0};
    }
    if (!hasExpectedHeader(input, heartbeat_message_type, heartbeat_body_size)) {
        return {DecodeStatus::error, 0};
    }
    if (input.size() < frame_size) {
        return {DecodeStatus::need_more_data, 0};
    }

    output.ts_ns = readUint64LittleEndian(
        input.subspan(frame_header_size, heartbeat_body_size));
    return {DecodeStatus::message_ready, frame_size};
}

std::size_t EncodeSessionControl(
    const SessionControlMessage& message,
    std::span<std::byte> output) {
    constexpr std::size_t frame_size = frame_header_size + session_control_body_size;
    if (output.size() < frame_size) {
        throw std::runtime_error("output buffer too small for session control frame");
    }
    if (static_cast<std::uint8_t>(message.state) >
        static_cast<std::uint8_t>(SessionState::close)) {
        throw std::runtime_error("invalid session control state");
    }

    encodeHeader(
        output.first(frame_header_size),
        session_control_message_type,
        session_control_body_size);
    writeUint64LittleEndian(
        output.subspan(frame_header_size, sizeof(message.ts_ns)),
        message.ts_ns);
    output[frame_header_size + sizeof(message.ts_ns)] =
        static_cast<std::byte>(message.state);
    return frame_size;
}

DecodeResult DecodeSessionControl(
    std::span<const std::byte> input,
    SessionControlMessage& output) {
    constexpr std::size_t frame_size = frame_header_size + session_control_body_size;
    if (input.size() < frame_header_size) {
        return {DecodeStatus::need_more_data, 0};
    }
    if (!hasExpectedHeader(
            input,
            session_control_message_type,
            session_control_body_size)) {
        return {DecodeStatus::error, 0};
    }
    if (input.size() < frame_size) {
        return {DecodeStatus::need_more_data, 0};
    }

    const std::uint8_t raw_state = std::to_integer<std::uint8_t>(input[12]);
    if (raw_state > static_cast<std::uint8_t>(SessionState::close)) {
        return {DecodeStatus::error, 0};
    }

    output.ts_ns = readUint64LittleEndian(input.subspan(frame_header_size, 8));
    output.state = static_cast<SessionState>(raw_state);
    return {DecodeStatus::message_ready, frame_size};
}

StreamDecodeResult MarketEventDecoder::Decode(
    std::span<const std::byte> input,
    std::vector<MarketEvent>& output) {
    pending_.insert(pending_.end(), input.begin(), input.end());

    std::size_t messages_decoded = 0;
    while (!pending_.empty()) {
        MarketEvent message{};
        const DecodeResult result = DecodeMarketEvent(pending_, message);

        if (result.status == DecodeStatus::need_more_data) {
            return {
                messages_decoded == 0
                    ? DecodeStatus::need_more_data
                    : DecodeStatus::message_ready,
                messages_decoded,
            };
        }

        if (result.status == DecodeStatus::error) {
            return {DecodeStatus::error, messages_decoded};
        }

        output.push_back(message);
        ++messages_decoded;
        pending_.erase(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(result.bytes_consumed));
    }

    return {
        messages_decoded == 0
            ? DecodeStatus::need_more_data
            : DecodeStatus::message_ready,
        messages_decoded,
    };
}

std::size_t MarketEventDecoder::BufferedBytes() const noexcept {
    return pending_.size();
}

StreamDecodeResult SessionControlDecoder::Decode(
    std::span<const std::byte> input,
    std::vector<SessionControlMessage>& output) {
    pending_.insert(pending_.end(), input.begin(), input.end());

    std::size_t messages_decoded = 0;
    while (!pending_.empty()) {
        SessionControlMessage message{};
        const DecodeResult result = DecodeSessionControl(pending_, message);

        if (result.status == DecodeStatus::need_more_data) {
            return {
                messages_decoded == 0
                    ? DecodeStatus::need_more_data
                    : DecodeStatus::message_ready,
                messages_decoded,
            };
        }

        if (result.status == DecodeStatus::error) {
            return {DecodeStatus::error, messages_decoded};
        }

        output.push_back(message);
        ++messages_decoded;
        pending_.erase(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(result.bytes_consumed));
    }

    return {
        messages_decoded == 0
            ? DecodeStatus::need_more_data
            : DecodeStatus::message_ready,
        messages_decoded,
    };
}

std::size_t SessionControlDecoder::BufferedBytes() const noexcept {
    return pending_.size();
}

std::size_t EncodeNewOrder(
    const NewOrderMessage& message,
    std::span<std::byte> output) {
    constexpr std::size_t frame_size = frame_header_size + new_order_body_size;
    if (output.size() < frame_size) {
        throw std::runtime_error("output buffer too small for NewOrder frame");
    }
    if (!isValid(message.status) || !isValid(message.side)) {
        throw std::runtime_error("invalid NewOrder enum value");
    }

    encodeHeader(output.first(frame_header_size), new_order_message_type, new_order_body_size);
    std::size_t offset = frame_header_size;

    writeUint64LittleEndian(output.subspan(offset, 8), message.client_order_id);
    offset += 8;
    std::memcpy(output.data() + offset, message.symbol, sizeof(message.symbol));
    offset += sizeof(message.symbol);
    output[offset++] = static_cast<std::byte>(message.status);
    writeUint64LittleEndian(output.subspan(offset, 8), message.ts_ns);
    offset += 8;
    writeInt64LittleEndian(output.subspan(offset, 8), message.trade_id);
    offset += 8;
    output[offset++] = static_cast<std::byte>(message.side);
    writeUint32LittleEndian(output.subspan(offset, 4), message.qty);
    offset += 4;
    writeInt64LittleEndian(output.subspan(offset, 8), message.limit_px);
    offset += 8;

    return offset;
}

DecodeResult DecodeNewOrder(
    std::span<const std::byte> input,
    NewOrderMessage& output) {
    constexpr std::size_t frame_size = frame_header_size + new_order_body_size;
    if (input.size() < frame_header_size) {
        return {DecodeStatus::need_more_data, 0};
    }
    if (!hasExpectedHeader(input, new_order_message_type, new_order_body_size)) {
        return {DecodeStatus::error, 0};
    }
    if (input.size() < frame_size) {
        return {DecodeStatus::need_more_data, 0};
    }

    std::size_t offset = frame_header_size;
    output.client_order_id = readUint64LittleEndian(input.subspan(offset, 8));
    offset += 8;
    std::memcpy(output.symbol, input.data() + offset, sizeof(output.symbol));
    offset += sizeof(output.symbol);
    output.status = static_cast<NewOrderStatus>(
        std::to_integer<unsigned char>(input[offset++]));
    output.ts_ns = readUint64LittleEndian(input.subspan(offset, 8));
    offset += 8;
    output.trade_id = readInt64LittleEndian(input.subspan(offset, 8));
    offset += 8;
    output.side = static_cast<OrderSide>(
        std::to_integer<unsigned char>(input[offset++]));
    output.qty = readUint32LittleEndian(input.subspan(offset, 4));
    offset += 4;
    output.limit_px = readInt64LittleEndian(input.subspan(offset, 8));
    offset += 8;

    if (!isValid(output.status) || !isValid(output.side)) {
        return {DecodeStatus::error, 0};
    }
    return {DecodeStatus::message_ready, offset};
}

std::size_t EncodeExecReport(
    const ExecReportMessage& message,
    std::span<std::byte> output) {
    constexpr std::size_t frame_size = frame_header_size + exec_report_body_size;
    if (output.size() < frame_size) {
        throw std::runtime_error("output buffer too small for ExecReport frame");
    }
    if (static_cast<std::uint8_t>(message.status) >
            static_cast<std::uint8_t>(ExecStatus::reject) ||
        static_cast<std::uint8_t>(message.reason_code) >
            static_cast<std::uint8_t>(RejectReason::throttle)) {
        throw std::runtime_error("invalid ExecReport enum value");
    }

    encodeHeader(output.first(frame_header_size), exec_report_message_type, exec_report_body_size);
    std::size_t offset = frame_header_size;

    writeUint64LittleEndian(output.subspan(offset, 8), message.client_order_id);
    offset += 8;
    writeUint64LittleEndian(output.subspan(offset, 8), message.ts_ns);
    offset += 8;
    output[offset++] = static_cast<std::byte>(message.status);
    writeUint32LittleEndian(output.subspan(offset, 4), message.filled_qty);
    offset += 4;
    writeInt64LittleEndian(output.subspan(offset, 8), message.avg_px);
    offset += 8;
    output[offset++] = static_cast<std::byte>(message.reason_code);

    return offset;
}

DecodeResult DecodeExecReport(
    std::span<const std::byte> input,
    ExecReportMessage& output) {
    constexpr std::size_t frame_size = frame_header_size + exec_report_body_size;
    if (input.size() < frame_header_size) {
        return {DecodeStatus::need_more_data, 0};
    }
    if (!hasExpectedHeader(input, exec_report_message_type, exec_report_body_size)) {
        return {DecodeStatus::error, 0};
    }
    if (input.size() < frame_size) {
        return {DecodeStatus::need_more_data, 0};
    }

    std::size_t offset = frame_header_size;
    output.client_order_id = readUint64LittleEndian(input.subspan(offset, 8));
    offset += 8;
    output.ts_ns = readUint64LittleEndian(input.subspan(offset, 8));
    offset += 8;
    output.status = static_cast<ExecStatus>(
        std::to_integer<std::uint8_t>(input[offset++]));
    output.filled_qty = readUint32LittleEndian(input.subspan(offset, 4));
    offset += 4;
    output.avg_px = readInt64LittleEndian(input.subspan(offset, 8));
    offset += 8;
    output.reason_code = static_cast<RejectReason>(
        std::to_integer<std::uint8_t>(input[offset++]));

    if (static_cast<std::uint8_t>(output.status) >
            static_cast<std::uint8_t>(ExecStatus::reject) ||
        static_cast<std::uint8_t>(output.reason_code) >
            static_cast<std::uint8_t>(RejectReason::throttle)) {
        return {DecodeStatus::error, 0};
    }
    return {DecodeStatus::message_ready, offset};
}

DecodeResult DecodeOrderEntryClientMessage(
    std::span<const std::byte> input,
    OrderEntryClientMessage& output) {
    if (input.size() < frame_header_size) {
        return {DecodeStatus::need_more_data, 0};
    }

    const std::uint8_t type = std::to_integer<std::uint8_t>(input[2]);
    if (type == new_order_message_type) {
        NewOrderMessage message{};
        const DecodeResult result = DecodeNewOrder(input, message);
        if (result.status == DecodeStatus::message_ready) {
            output = message;
        }
        return result;
    }
    if (type == exec_report_message_type) {
        ExecReportMessage message{};
        const DecodeResult result = DecodeExecReport(input, message);
        if (result.status == DecodeStatus::message_ready) {
            output = message;
        }
        return result;
    }
    if (type == heartbeat_message_type) {
        HeartbeatMessage message{};
        const DecodeResult result = DecodeHeartbeat(input, message);
        if (result.status == DecodeStatus::message_ready) {
            output = message;
        }
        return result;
    }
    if (type == session_control_message_type) {
        SessionControlMessage message{};
        const DecodeResult result = DecodeSessionControl(input, message);
        if (result.status == DecodeStatus::message_ready) {
            output = message;
        }
        return result;
    }
    return {DecodeStatus::error, 0};
}

StreamDecodeResult ClientSideDecoder::Decode(
    std::span<const std::byte> input,
    std::vector<OrderEntryClientMessage>& output) {
    pending_.insert(pending_.end(), input.begin(), input.end());

    std::size_t messages_decoded = 0;
    while (!pending_.empty()) {
        OrderEntryClientMessage message{};
        const DecodeResult result = DecodeOrderEntryClientMessage(pending_, message);
        if (result.status == DecodeStatus::need_more_data) {
            return {
                messages_decoded == 0
                    ? DecodeStatus::need_more_data
                    : DecodeStatus::message_ready,
                messages_decoded,
            };
        }
        if (result.status == DecodeStatus::error) {
            return {DecodeStatus::error, messages_decoded};
        }

        output.push_back(message);
        ++messages_decoded;
        pending_.erase(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(result.bytes_consumed));
    }

    return {
        messages_decoded == 0
            ? DecodeStatus::need_more_data
            : DecodeStatus::message_ready,
        messages_decoded,
    };
}

std::size_t ClientSideDecoder::BufferedBytes() const noexcept {
    return pending_.size();
}


} // namespace slipstream::codec
