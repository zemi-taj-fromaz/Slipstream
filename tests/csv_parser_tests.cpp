#include "parser.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>

namespace {

constexpr std::string_view header =
    "Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty,Price,Qty";

class TemporaryCsv {
public:
    explicit TemporaryCsv(std::string_view contents) {
        static std::atomic_uint64_t sequence{0};
        path_ = std::filesystem::temp_directory_path() /
                ("slipstream_csv_parser_" +
                 std::to_string(sequence.fetch_add(1)) + ".csv");

        std::ofstream file{path_, std::ios::binary};
        if (!file) {
            throw std::runtime_error("failed to create temporary CSV fixture");
        }
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    ~TemporaryCsv() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporaryCsv(const TemporaryCsv&) = delete;
    TemporaryCsv& operator=(const TemporaryCsv&) = delete;

    [[nodiscard]] const char* path() const {
        path_string_ = path_.string();
        return path_string_.c_str();
    }

private:
    std::filesystem::path path_;
    mutable std::string path_string_;
};

TEST(CsvParser, ParsesQuotesAndTrades) {
    const TemporaryCsv csv{
        std::string{"# Sample Market Data CSV\n# ignored metadata\n"} +
        std::string{header} + "\n" +
        "09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,\n" +
        "09:30:00.190,T,SYNTH2,,,,,248.53,65"};

    const auto events = parse_csv(csv.path());

    ASSERT_EQ(events.size(), 2U);

    EXPECT_EQ(events[0].ts, 34'200'003'000'000ULL);
    EXPECT_STREQ(events[0].symbol, "SYNTH3");
    ASSERT_TRUE(std::holds_alternative<Quote>(events[0].payload));
    const auto& quote = std::get<Quote>(events[0].payload);
    EXPECT_EQ(quote.bid_price, 873'700);
    EXPECT_EQ(quote.bid_qty, 55U);
    EXPECT_EQ(quote.ask_price, 874'900);
    EXPECT_EQ(quote.ask_qty, 50U);

    EXPECT_EQ(events[1].ts, 34'200'190'000'000ULL);
    EXPECT_STREQ(events[1].symbol, "SYNTH2");
    ASSERT_TRUE(std::holds_alternative<Trade>(events[1].payload));
    const auto& trade = std::get<Trade>(events[1].payload);
    EXPECT_EQ(trade.price, 2'485'300);
    EXPECT_EQ(trade.qty, 65U);
}

TEST(CsvParser, AcceptsNanosecondTimestamp) {
    const TemporaryCsv csv{
        std::string{header} + "\n" +
        "123456789,Q,SYNTH1,1.2,10,1.2345,20,,\n"};

    const auto events = parse_csv(csv.path());

    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].ts, 123'456'789ULL);
    EXPECT_STREQ(events[0].symbol, "SYNTH1");

    const auto& quote = std::get<Quote>(events[0].payload);
    EXPECT_EQ(quote.bid_price, 12'000);
    EXPECT_EQ(quote.ask_price, 12'345);
}

TEST(CsvParser, AcceptsWindowsLineEndings) {
    const TemporaryCsv csv{
        std::string{"# metadata\r\n"} + std::string{header} + "\r\n" +
        "09:30:00.003,T,SYNTH1,,,,,101.25,100\r\n"};

    const auto events = parse_csv(csv.path());

    ASSERT_EQ(events.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<Trade>(events[0].payload));
}

TEST(CsvParser, ReturnsEmptyVectorWhenHeaderHasNoRows) {
    const TemporaryCsv csv{std::string{header} + "\n"};

    EXPECT_TRUE(parse_csv(csv.path()).empty());
}

TEST(CsvParser, RejectsMissingHeader) {
    const TemporaryCsv csv{"# metadata only\n09:30:00.003,T,SYNTH1,,,,,101.25,100\n"};

    EXPECT_THROW(static_cast<void>(parse_csv(csv.path())), std::runtime_error);
}

TEST(CsvParser, RejectsWrongColumnCount) {
    const TemporaryCsv csv{
        std::string{header} + "\n" +
        "09:30:00.003,T,SYNTH1,101.25,100\n"};

    EXPECT_THROW(static_cast<void>(parse_csv(csv.path())), std::runtime_error);
}

TEST(CsvParser, RejectsUnknownEventType) {
    const TemporaryCsv csv{
        std::string{header} + "\n" +
        "09:30:00.003,X,SYNTH1,,,,,101.25,100\n"};

    EXPECT_THROW(static_cast<void>(parse_csv(csv.path())), std::runtime_error);
}

TEST(CsvParser, RejectsInvalidNumericField) {
    const TemporaryCsv csv{
        std::string{header} + "\n" +
        "09:30:00.003,Q,SYNTH1,101.24,invalid,101.26,400,,\n"};

    EXPECT_THROW(static_cast<void>(parse_csv(csv.path())), std::runtime_error);
}

} // namespace
