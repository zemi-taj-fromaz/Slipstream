#include "slipstream_codec/market_data_codec.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <variant>
#include <vector>

namespace {

using namespace slipstream::codec;

MarketEvent MakeQuote() {
    MarketEvent event{
        .ts = 0x0102030405060708ULL,
        .symbol = {},
        .payload = Quote{
            .bid_price = 0x1112131415161718LL,
            .bid_qty = 0x0A0B0C0D,
            .ask_price = 0x2122232425262728LL,
            .ask_qty = 0x1A1B1C1D,
        },
    };
    std::memcpy(event.symbol, "SYNTH1", 6);
    return event;
}

MarketEvent MakeTrade() {
    MarketEvent event{
        .ts = 0x3132333435363738ULL,
        .symbol = {},
        .payload = Trade{
            .price = 0x1112131415161718LL,
            .id = 0x4142434445464748LL,
            .qty = 0x01020304,
            .aggressor = 'B',
        },
    };
    std::memcpy(event.symbol, "SYNTH2", 6);
    return event;
}

void ExpectSameEvent(const MarketEvent& expected, const MarketEvent& actual) {
    EXPECT_EQ(expected.ts, actual.ts);
    EXPECT_EQ(std::memcmp(expected.symbol, actual.symbol, sizeof(expected.symbol)), 0);
    ASSERT_EQ(expected.payload.index(), actual.payload.index());

    if (std::holds_alternative<Quote>(expected.payload)) {
        const auto& expected_quote = std::get<Quote>(expected.payload);
        const auto& actual_quote = std::get<Quote>(actual.payload);
        EXPECT_EQ(expected_quote.bid_price, actual_quote.bid_price);
        EXPECT_EQ(expected_quote.bid_qty, actual_quote.bid_qty);
        EXPECT_EQ(expected_quote.ask_price, actual_quote.ask_price);
        EXPECT_EQ(expected_quote.ask_qty, actual_quote.ask_qty);
    } else {
        const auto& expected_trade = std::get<Trade>(expected.payload);
        const auto& actual_trade = std::get<Trade>(actual.payload);
        EXPECT_EQ(expected_trade.price, actual_trade.price);
        EXPECT_EQ(expected_trade.id, actual_trade.id);
        EXPECT_EQ(expected_trade.qty, actual_trade.qty);
        EXPECT_EQ(expected_trade.aggressor, actual_trade.aggressor);
    }
}

std::vector<std::byte> Encode(const MarketEvent& event) {
    std::array<std::byte, max_market_data_frame_size> storage{};
    const std::size_t bytes_written = EncodeMarketEvent(event, storage);
    return {storage.begin(), storage.begin() + static_cast<std::ptrdiff_t>(bytes_written)};
}

TEST(MarketDataCodec, EncodesQuoteUsingSpecifiedLittleEndianLayout) {
    const auto frame = Encode(MakeQuote());

    const std::array<std::byte, 48> expected{
        std::byte{0x2C}, std::byte{0x00}, std::byte{0x01}, std::byte{0x01},
        std::byte{'S'}, std::byte{'Y'}, std::byte{'N'}, std::byte{'T'},
        std::byte{'H'}, std::byte{'1'}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
        std::byte{0x0D}, std::byte{0x0C}, std::byte{0x0B}, std::byte{0x0A},
        std::byte{0x18}, std::byte{0x17}, std::byte{0x16}, std::byte{0x15},
        std::byte{0x14}, std::byte{0x13}, std::byte{0x12}, std::byte{0x11},
        std::byte{0x1D}, std::byte{0x1C}, std::byte{0x1B}, std::byte{0x1A},
        std::byte{0x28}, std::byte{0x27}, std::byte{0x26}, std::byte{0x25},
        std::byte{0x24}, std::byte{0x23}, std::byte{0x22}, std::byte{0x21},
    };

    EXPECT_EQ(frame.size(), expected.size());
    EXPECT_TRUE(std::equal(frame.begin(), frame.end(), expected.begin()));
}

TEST(MarketDataCodec, EncodesTradeUsingSpecifiedLittleEndianLayout) {
    const auto frame = Encode(MakeTrade());

    const std::array<std::byte, 45> expected{
        std::byte{0x29}, std::byte{0x00}, std::byte{0x02}, std::byte{0x01},
        std::byte{'S'}, std::byte{'Y'}, std::byte{'N'}, std::byte{'T'},
        std::byte{'H'}, std::byte{'2'}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x38}, std::byte{0x37}, std::byte{0x36}, std::byte{0x35},
        std::byte{0x34}, std::byte{0x33}, std::byte{0x32}, std::byte{0x31},
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
        std::byte{0x18}, std::byte{0x17}, std::byte{0x16}, std::byte{0x15},
        std::byte{0x14}, std::byte{0x13}, std::byte{0x12}, std::byte{0x11},
        std::byte{'B'},
        std::byte{0x48}, std::byte{0x47}, std::byte{0x46}, std::byte{0x45},
        std::byte{0x44}, std::byte{0x43}, std::byte{0x42}, std::byte{0x41},
    };

    EXPECT_EQ(frame.size(), expected.size());
    EXPECT_TRUE(std::equal(frame.begin(), frame.end(), expected.begin()));
}

TEST(MarketDataCodec, RoundTripsQuoteAndTrade) {
    for (const MarketEvent& expected : {MakeQuote(), MakeTrade()}) {
        const auto frame = Encode(expected);
        MarketEvent decoded{};

        const DecodeResult result = DecodeMarketEvent(frame, decoded);

        EXPECT_EQ(result.status, DecodeStatus::message_ready);
        EXPECT_EQ(result.bytes_consumed, frame.size());
        ExpectSameEvent(expected, decoded);
    }
}

TEST(MarketDataCodec, RoundTripsNegativeFixedPointPrice) {
    MarketEvent expected = MakeTrade();
    std::get<Trade>(expected.payload).price = -1'012'500;
    const auto frame = Encode(expected);
    MarketEvent decoded{};

    const DecodeResult result = DecodeMarketEvent(frame, decoded);

    ASSERT_EQ(result.status, DecodeStatus::message_ready);
    ExpectSameEvent(expected, decoded);
}

TEST(MarketDataCodec, RejectsOutputBufferThatIsTooSmall) {
    std::array<std::byte, 47> output{};
    EXPECT_THROW(
        static_cast<void>(EncodeMarketEvent(MakeQuote(), output)),
        std::runtime_error);
}

TEST(MarketDataCodec, RejectsUnsupportedProtocolVersion) {
    auto frame = Encode(MakeQuote());
    frame[3] = std::byte{2};
    MarketEvent decoded{};

    const DecodeResult result = DecodeMarketEvent(frame, decoded);

    EXPECT_EQ(result.status, DecodeStatus::error);
    EXPECT_EQ(result.bytes_consumed, 0U);
}

TEST(MarketDataStreamDecoder, ReassemblesQuoteAtEverySplitBoundary) {
    const MarketEvent expected = MakeQuote();
    const auto frame = Encode(expected);

    for (std::size_t split = 0; split <= frame.size(); ++split) {
        SCOPED_TRACE(split);
        MarketDataStreamDecoder decoder;
        std::vector<MarketDataMessage> decoded;

        const auto first = decoder.Consume(
            std::span<const std::byte>{frame}.first(split),
            decoded);
        const auto second = decoder.Consume(
            std::span<const std::byte>{frame}.subspan(split),
            decoded);

        ASSERT_EQ(decoded.size(), 1U);
        ASSERT_TRUE(std::holds_alternative<MarketEvent>(decoded.front()));
        ExpectSameEvent(expected, std::get<MarketEvent>(decoded.front()));
        EXPECT_EQ(decoder.BufferedBytes(), 0U);
        EXPECT_EQ(first.messages_decoded + second.messages_decoded, 1U);
    }
}

TEST(MarketDataStreamDecoder, ReassemblesTradeOneByteAtATime) {
    const MarketEvent expected = MakeTrade();
    const auto frame = Encode(expected);
    MarketDataStreamDecoder decoder;
    std::vector<MarketDataMessage> decoded;

    for (const std::byte byte : frame) {
        const std::array chunk{byte};
        static_cast<void>(decoder.Consume(chunk, decoded));
    }

    ASSERT_EQ(decoded.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<MarketEvent>(decoded.front()));
    ExpectSameEvent(expected, std::get<MarketEvent>(decoded.front()));
    EXPECT_EQ(decoder.BufferedBytes(), 0U);
}

TEST(MarketDataStreamDecoder, DecodesCoalescedFramesAndRetainsPartialFrame) {
    const MarketEvent quote = MakeQuote();
    const MarketEvent trade = MakeTrade();
    const auto quote_frame = Encode(quote);
    const auto trade_frame = Encode(trade);

    std::vector<std::byte> first_chunk = quote_frame;
    first_chunk.insert(
        first_chunk.end(),
        trade_frame.begin(),
        trade_frame.begin() + 10);

    MarketDataStreamDecoder decoder;
    std::vector<MarketDataMessage> decoded;
    const auto first = decoder.Consume(first_chunk, decoded);

    ASSERT_EQ(decoded.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<MarketEvent>(decoded[0]));
    ExpectSameEvent(quote, std::get<MarketEvent>(decoded[0]));
    EXPECT_EQ(first.messages_decoded, 1U);
    EXPECT_EQ(decoder.BufferedBytes(), 10U);

    const auto second = decoder.Consume(
        std::span<const std::byte>{trade_frame}.subspan(10),
        decoded);

    ASSERT_EQ(decoded.size(), 2U);
    ASSERT_TRUE(std::holds_alternative<MarketEvent>(decoded[1]));
    ExpectSameEvent(trade, std::get<MarketEvent>(decoded[1]));
    EXPECT_EQ(second.messages_decoded, 1U);
    EXPECT_EQ(decoder.BufferedBytes(), 0U);
}

TEST(MarketDataCodec, EncodesAndDecodesHeartbeat) {
    constexpr HeartbeatMessage expected{.ts_ns = 0x0102030405060708ULL};
    std::array<std::byte, 12> frame{};

    const std::size_t bytes_written = EncodeHeartbeat(expected, frame);

    const std::array<std::byte, 12> expected_bytes{
        std::byte{0x08}, std::byte{0x00}, std::byte{0x03}, std::byte{0x01},
        std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
    };
    EXPECT_EQ(bytes_written, expected_bytes.size());
    EXPECT_EQ(frame, expected_bytes);

    HeartbeatMessage decoded{};
    const DecodeResult result = DecodeHeartbeat(frame, decoded);
    EXPECT_EQ(result.status, DecodeStatus::message_ready);
    EXPECT_EQ(result.bytes_consumed, frame.size());
    EXPECT_EQ(decoded.ts_ns, expected.ts_ns);
}

TEST(MarketDataCodec, EncodesAndDecodesSessionControl) {
    constexpr SessionControlMessage expected{
        .ts_ns = 0x1112131415161718ULL,
        .state = SessionState::halt,
    };
    std::array<std::byte, 13> frame{};

    const std::size_t bytes_written = EncodeSessionControl(expected, frame);

    const std::array<std::byte, 13> expected_bytes{
        std::byte{0x09}, std::byte{0x00}, std::byte{0x04}, std::byte{0x01},
        std::byte{0x18}, std::byte{0x17}, std::byte{0x16}, std::byte{0x15},
        std::byte{0x14}, std::byte{0x13}, std::byte{0x12}, std::byte{0x11},
        std::byte{0x01},
    };
    EXPECT_EQ(bytes_written, expected_bytes.size());
    EXPECT_EQ(frame, expected_bytes);

    SessionControlMessage decoded{};
    const DecodeResult result = DecodeSessionControl(frame, decoded);
    EXPECT_EQ(result.status, DecodeStatus::message_ready);
    EXPECT_EQ(result.bytes_consumed, frame.size());
    EXPECT_EQ(decoded.ts_ns, expected.ts_ns);
    EXPECT_EQ(decoded.state, expected.state);
}

TEST(MarketDataCodec, RejectsInvalidSessionControlState) {
    std::array<std::byte, 13> frame{};
    static_cast<void>(EncodeSessionControl(
        SessionControlMessage{.ts_ns = 42, .state = SessionState::open},
        frame));
    frame[12] = std::byte{3};

    SessionControlMessage decoded{};
    const DecodeResult result = DecodeSessionControl(frame, decoded);

    EXPECT_EQ(result.status, DecodeStatus::error);
    EXPECT_EQ(result.bytes_consumed, 0U);
}

TEST(MarketDataStreamDecoder, ReassemblesHeartbeatAndSessionControlFromChunks) {
    constexpr HeartbeatMessage heartbeat{.ts_ns = 123'456};
    constexpr SessionControlMessage session{
        .ts_ns = 789'012,
        .state = SessionState::close,
    };
    std::array<std::byte, 12> heartbeat_frame{};
    std::array<std::byte, 13> session_frame{};
    static_cast<void>(EncodeHeartbeat(heartbeat, heartbeat_frame));
    static_cast<void>(EncodeSessionControl(session, session_frame));

    std::vector<std::byte> stream{heartbeat_frame.begin(), heartbeat_frame.end()};
    stream.insert(stream.end(), session_frame.begin(), session_frame.end());

    MarketDataStreamDecoder decoder;
    std::vector<MarketDataMessage> decoded;
    constexpr std::size_t chunk_size = 3;

    for (std::size_t offset = 0; offset < stream.size(); offset += chunk_size) {
        const std::size_t count = std::min(chunk_size, stream.size() - offset);
        static_cast<void>(decoder.Consume(
            std::span<const std::byte>{stream}.subspan(offset, count),
            decoded));
    }

    ASSERT_EQ(decoded.size(), 2U);
    ASSERT_TRUE(std::holds_alternative<HeartbeatMessage>(decoded[0]));
    EXPECT_EQ(std::get<HeartbeatMessage>(decoded[0]).ts_ns, heartbeat.ts_ns);
    ASSERT_TRUE(std::holds_alternative<SessionControlMessage>(decoded[1]));
    const auto& decoded_session = std::get<SessionControlMessage>(decoded[1]);
    EXPECT_EQ(decoded_session.ts_ns, session.ts_ns);
    EXPECT_EQ(decoded_session.state, session.state);
    EXPECT_EQ(decoder.BufferedBytes(), 0U);
}

} // namespace
