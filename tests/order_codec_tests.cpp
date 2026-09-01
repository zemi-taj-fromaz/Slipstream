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

NewOrderMessage MakeNewOrder() {
    NewOrderMessage message{
        .client_order_id = 0x0102030405060708ULL,
        .symbol = {},
        .status = NewOrderStatus::accepted,
        .ts_ns = 0x1112131415161718ULL,
        .trade_id = 0x2122232425262728LL,
        .side = OrderSide::buy,
        .qty = 0x31323334,
        .limit_px = 0x4142434445464748LL,
    };
    std::memcpy(message.symbol, "SYNTH1", 6);
    return message;
}

ExecReportMessage MakeExecReport() {
    return {
        .client_order_id = 0x0102030405060708ULL,
        .ts_ns = 0x1112131415161718ULL,
        .status = ExecStatus::partial,
        .filled_qty = 0x21222324,
        .avg_px = 0x3132333435363738LL,
        .reason_code = RejectReason::price,
    };
}

void ExpectSame(const NewOrderMessage& expected, const NewOrderMessage& actual) {
    EXPECT_EQ(expected.client_order_id, actual.client_order_id);
    EXPECT_EQ(std::memcmp(expected.symbol, actual.symbol, sizeof(expected.symbol)), 0);
    EXPECT_EQ(expected.status, actual.status);
    EXPECT_EQ(expected.ts_ns, actual.ts_ns);
    EXPECT_EQ(expected.trade_id, actual.trade_id);
    EXPECT_EQ(expected.side, actual.side);
    EXPECT_EQ(expected.qty, actual.qty);
    EXPECT_EQ(expected.limit_px, actual.limit_px);
}

void ExpectSame(const ExecReportMessage& expected, const ExecReportMessage& actual) {
    EXPECT_EQ(expected.client_order_id, actual.client_order_id);
    EXPECT_EQ(expected.ts_ns, actual.ts_ns);
    EXPECT_EQ(expected.status, actual.status);
    EXPECT_EQ(expected.filled_qty, actual.filled_qty);
    EXPECT_EQ(expected.avg_px, actual.avg_px);
    EXPECT_EQ(expected.reason_code, actual.reason_code);
}

std::vector<std::byte> Encode(const NewOrderMessage& message) {
    std::array<std::byte, max_order_frame_size> storage{};
    const std::size_t bytes_written = EncodeNewOrder(message, storage);
    return {storage.begin(), storage.begin() + static_cast<std::ptrdiff_t>(bytes_written)};
}

std::vector<std::byte> Encode(const ExecReportMessage& message) {
    std::array<std::byte, max_order_frame_size> storage{};
    const std::size_t bytes_written = EncodeExecReport(message, storage);
    return {storage.begin(), storage.begin() + static_cast<std::ptrdiff_t>(bytes_written)};
}

TEST(OrderCodec, UsesFiftyByteNewOrderBodyFromListedFields) {
    EXPECT_EQ(new_order_body_size, 50U);
    EXPECT_EQ(Encode(MakeNewOrder()).size(), 54U);
}

TEST(OrderCodec, EncodesNewOrderUsingSpecifiedLittleEndianLayout) {
    const auto frame = Encode(MakeNewOrder());

    const std::array<std::byte, 54> expected{
        std::byte{0x32}, std::byte{0x00}, std::byte{0x0A}, std::byte{0x01},
        std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
        std::byte{'S'}, std::byte{'Y'}, std::byte{'N'}, std::byte{'T'},
        std::byte{'H'}, std::byte{'1'}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{'A'},
        std::byte{0x18}, std::byte{0x17}, std::byte{0x16}, std::byte{0x15},
        std::byte{0x14}, std::byte{0x13}, std::byte{0x12}, std::byte{0x11},
        std::byte{0x28}, std::byte{0x27}, std::byte{0x26}, std::byte{0x25},
        std::byte{0x24}, std::byte{0x23}, std::byte{0x22}, std::byte{0x21},
        std::byte{'B'},
        std::byte{0x34}, std::byte{0x33}, std::byte{0x32}, std::byte{0x31},
        std::byte{0x48}, std::byte{0x47}, std::byte{0x46}, std::byte{0x45},
        std::byte{0x44}, std::byte{0x43}, std::byte{0x42}, std::byte{0x41},
    };

    EXPECT_EQ(frame.size(), expected.size());
    EXPECT_TRUE(std::equal(frame.begin(), frame.end(), expected.begin()));
}

TEST(OrderCodec, EncodesExecReportUsingSpecifiedLittleEndianLayout) {
    const auto frame = Encode(MakeExecReport());

    const std::array<std::byte, 34> expected{
        std::byte{0x1E}, std::byte{0x00}, std::byte{0x0B}, std::byte{0x01},
        std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
        std::byte{0x18}, std::byte{0x17}, std::byte{0x16}, std::byte{0x15},
        std::byte{0x14}, std::byte{0x13}, std::byte{0x12}, std::byte{0x11},
        std::byte{0x02},
        std::byte{0x24}, std::byte{0x23}, std::byte{0x22}, std::byte{0x21},
        std::byte{0x38}, std::byte{0x37}, std::byte{0x36}, std::byte{0x35},
        std::byte{0x34}, std::byte{0x33}, std::byte{0x32}, std::byte{0x31},
        std::byte{0x02},
    };

    EXPECT_EQ(frame.size(), expected.size());
    EXPECT_TRUE(std::equal(frame.begin(), frame.end(), expected.begin()));
}

TEST(OrderCodec, RoundTripsNewOrderAndExecReport) {
    const NewOrderMessage expected_order = MakeNewOrder();
    const auto order_frame = Encode(expected_order);
    NewOrderMessage decoded_order{};

    const auto order_result = DecodeNewOrder(order_frame, decoded_order);
    ASSERT_EQ(order_result.status, DecodeStatus::message_ready);
    EXPECT_EQ(order_result.bytes_consumed, order_frame.size());
    ExpectSame(expected_order, decoded_order);

    const ExecReportMessage expected_report = MakeExecReport();
    const auto report_frame = Encode(expected_report);
    ExecReportMessage decoded_report{};

    const auto report_result = DecodeExecReport(report_frame, decoded_report);
    ASSERT_EQ(report_result.status, DecodeStatus::message_ready);
    EXPECT_EQ(report_result.bytes_consumed, report_frame.size());
    ExpectSame(expected_report, decoded_report);
}

TEST(OrderCodec, RoundTripsNegativePricesAndTradeId) {
    NewOrderMessage expected_order = MakeNewOrder();
    expected_order.trade_id = -42;
    expected_order.limit_px = -1'012'500;
    NewOrderMessage decoded_order{};
    const auto order_frame = Encode(expected_order);
    ASSERT_EQ(
        DecodeNewOrder(order_frame, decoded_order).status,
        DecodeStatus::message_ready);
    ExpectSame(expected_order, decoded_order);

    ExecReportMessage expected_report = MakeExecReport();
    expected_report.avg_px = -1'012'500;
    ExecReportMessage decoded_report{};
    const auto report_frame = Encode(expected_report);
    ASSERT_EQ(
        DecodeExecReport(report_frame, decoded_report).status,
        DecodeStatus::message_ready);
    ExpectSame(expected_report, decoded_report);
}

TEST(OrderCodec, RejectsInvalidHeaderAndEnumValues) {
    auto order_frame = Encode(MakeNewOrder());
    order_frame[3] = std::byte{2};
    NewOrderMessage decoded_order{};
    EXPECT_EQ(
        DecodeNewOrder(order_frame, decoded_order).status,
        DecodeStatus::error);

    order_frame = Encode(MakeNewOrder());
    order_frame[24] = std::byte{'X'};
    EXPECT_EQ(
        DecodeNewOrder(order_frame, decoded_order).status,
        DecodeStatus::error);

    auto report_frame = Encode(MakeExecReport());
    report_frame[20] = std::byte{4};
    ExecReportMessage decoded_report{};
    EXPECT_EQ(
        DecodeExecReport(report_frame, decoded_report).status,
        DecodeStatus::error);
}

TEST(OrderCodec, RejectsOutputBuffersThatAreTooSmall) {
    std::array<std::byte, 53> order_output{};
    EXPECT_THROW(
        static_cast<void>(EncodeNewOrder(MakeNewOrder(), order_output)),
        std::runtime_error);

    std::array<std::byte, 33> report_output{};
    EXPECT_THROW(
        static_cast<void>(EncodeExecReport(MakeExecReport(), report_output)),
        std::runtime_error);
}

TEST(ClientSideDecoder, ReassemblesNewOrderAtEverySplitBoundary) {
    const NewOrderMessage expected = MakeNewOrder();
    const auto frame = Encode(expected);

    for (std::size_t split = 0; split <= frame.size(); ++split) {
        SCOPED_TRACE(split);
        ClientSideDecoder decoder;
        std::vector<OrderEntryClientMessage> decoded;

        const auto first = decoder.Decode(
            std::span<const std::byte>{frame}.first(split),
            decoded);
        const auto second = decoder.Decode(
            std::span<const std::byte>{frame}.subspan(split),
            decoded);

        ASSERT_EQ(decoded.size(), 1U);
        ASSERT_TRUE(std::holds_alternative<NewOrderMessage>(decoded[0]));
        ExpectSame(expected, std::get<NewOrderMessage>(decoded[0]));
        EXPECT_EQ(first.messages_decoded + second.messages_decoded, 1U);
        EXPECT_EQ(decoder.BufferedBytes(), 0U);
    }
}

TEST(ClientSideDecoder, DecodesNewOrderThenPartialHeartbeat) {
    const NewOrderMessage order = MakeNewOrder();
    const auto order_frame = Encode(order);
    constexpr HeartbeatMessage heartbeat{.ts_ns = 123'456};
    std::array<std::byte, frame_header_size + heartbeat_body_size>
        heartbeat_frame{};
    static_cast<void>(EncodeHeartbeat(heartbeat, heartbeat_frame));

    std::vector<std::byte> first_chunk = order_frame;
    first_chunk.insert(
        first_chunk.end(),
        heartbeat_frame.begin(),
        heartbeat_frame.begin() + 7);

    ClientSideDecoder decoder;
    std::vector<OrderEntryClientMessage> decoded;
    const auto first = decoder.Decode(first_chunk, decoded);

    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(first.messages_decoded, 1U);
    EXPECT_EQ(decoder.BufferedBytes(), 7U);

    const auto second = decoder.Decode(
        std::span<const std::byte>{heartbeat_frame}.subspan(7),
        decoded);

    ASSERT_EQ(decoded.size(), 2U);
    ASSERT_TRUE(std::holds_alternative<NewOrderMessage>(decoded[0]));
    ASSERT_TRUE(std::holds_alternative<HeartbeatMessage>(decoded[1]));
    ExpectSame(order, std::get<NewOrderMessage>(decoded[0]));
    EXPECT_EQ(
        std::get<HeartbeatMessage>(decoded[1]).ts_ns,
        heartbeat.ts_ns);
    EXPECT_EQ(second.messages_decoded, 1U);
    EXPECT_EQ(decoder.BufferedBytes(), 0U);
}

TEST(ClientSideDecoder, DecodesExecReport) {
    const ExecReportMessage expected = MakeExecReport();
    const auto frame = Encode(expected);
    ClientSideDecoder decoder;
    std::vector<OrderEntryClientMessage> decoded;

    const auto result = decoder.Decode(frame, decoded);

    ASSERT_EQ(result.status, DecodeStatus::message_ready);
    ASSERT_EQ(result.messages_decoded, 1U);
    ASSERT_EQ(decoded.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<ExecReportMessage>(decoded.front()));
    ExpectSame(expected, std::get<ExecReportMessage>(decoded.front()));
    EXPECT_EQ(decoder.BufferedBytes(), 0U);
}

} // namespace
