#include "message_processor.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

namespace {

MarketEvent QuoteEvent() {
    MarketEvent event{
        .ts = 123'000'000,
        .symbol = {},
        .payload = Quote{
            .bid_price = 1'012'300,
            .bid_qty = 100,
            .ask_price = 1'012'500,
            .ask_qty = 200,
        },
    };
    std::strcpy(event.symbol, "SYNTH1");
    return event;
}

MarketEvent TradeEvent() {
    MarketEvent event{
        .ts = 456'000'000,
        .symbol = {},
        .payload = Trade{
            .price = 1'012'400,
            .qty = 75,
        },
    };
    std::strcpy(event.symbol, "SYNTH1");
    return event;
}

TEST(ConsoleProcessMsg, PrintsQuote) {
    ConsoleProcessMsg processor;

    testing::internal::CaptureStdout();
    processor.Sink(QuoteEvent());
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(
        output,
        "ts=123000000 symbol=SYNTH1 type=Q "
        "bid=1012300x100 ask=1012500x200\n");
}

TEST(ConsoleProcessMsg, PrintsTrade) {
    ConsoleProcessMsg processor;

    testing::internal::CaptureStdout();
    processor.Sink(TradeEvent());
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(
        output,
        "ts=456000000 symbol=SYNTH1 type=T price=1012400 qty=75\n");
}

class CountingProcessor final : public IProcessMsgClass {
public:
    void Sink(const MarketEvent&) override {
        ++count;
    }

    int count{0};
};

TEST(FanoutProcessMsg, ForwardsToBothProcessors) {
    CountingProcessor first;
    CountingProcessor second;
    FanoutProcessMsg fanout{first, second};

    fanout.Sink(QuoteEvent());

    EXPECT_EQ(first.count, 1);
    EXPECT_EQ(second.count, 1);
}

} // namespace
