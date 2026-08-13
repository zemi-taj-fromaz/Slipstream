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
    return static_cast<std::int64_t>(readUint64LittleEndian(bytes));
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


} // namespace slipstream::codec
