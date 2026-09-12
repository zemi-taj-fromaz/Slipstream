#include "Engine.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <variant>
#include <vector>

namespace {

MarketEvent MakeQuote(const std::uint64_t ts, const std::int64_t ask_price) {
    MarketEvent event{
        .ts = ts,
        .symbol = {},
        .payload =
            Quote{
                .bid_price = 999'000,
                .ask_price = ask_price,
                .bid_qty = 1'000,
                .ask_qty = 1'000,
            },
    };
    std::memcpy(event.symbol, "SYNTH1", 6);
    return event;
}

MarketEvent MakeTrade() {
    MarketEvent event{
        .ts = 1'000'000'000ULL,
        .symbol = {},
        .payload =
            Trade{
                .price = 997'000,
                .id = 77,
                .qty = 100,
                .aggressor = '?',
            },
    };
    std::memcpy(event.symbol, "SYNTH1", 6);
    return event;
}

class EngineModes : public ::testing::TestWithParam<ExecutionMode> {};

TEST_P(EngineModes, ProcessesIngressAndProducesOrderLifecycle) {
    SlipstreamConfig config{};
    config.execution_mode = GetParam();
    config.vwap_window_ms = 1'000;
    config.max_quantity = 500;
    config.participation_cap = 1.0;
    config.band_bps = 0.0;

    slipstream::MarketEventQueue ingress;
    slipstream::OrderEntryQueue egress;
    std::atomic<std::uint64_t> ingress_generation{0};
    std::atomic<std::uint64_t> notifications{0};

    Engine engine{config, ingress, egress, ingress_generation,
                  [&notifications] { notifications.fetch_add(1, std::memory_order_relaxed); }};

    const auto enqueue = [&](MarketEvent event, const std::uint64_t received) {
        const slipstream::InboundEvent inbound{
            .message = event,
            .received_at_ns = received,
        };
        ASSERT_TRUE(ingress.push(inbound));
        ingress_generation.fetch_add(1, std::memory_order_release);
    };

    enqueue(MakeQuote(0, 1'000'000), 1);
    for (std::uint64_t index = 0; index < 10; ++index) {
        const std::uint64_t timestamp = index == 9 ? 1'000'000'000ULL : index * 100'000'000ULL;
        enqueue(MakeQuote(timestamp, 1'000'100 + static_cast<std::int64_t>(index) * 100), index + 2);
    }
    enqueue(MakeTrade(), 100);

    std::jthread engine_thread{[&engine] { engine.Run(); }};

    std::vector<slipstream::OutboundMessage> outbound;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (outbound.size() < 3 && std::chrono::steady_clock::now() < deadline) {
        slipstream::OutboundMessage message{};
        if (egress.pop(message)) {
            outbound.push_back(message);
        } else {
            std::this_thread::yield();
        }
    }

    engine.Stop();
    engine_thread.join();

    ASSERT_EQ(outbound.size(), 3U);
    ASSERT_TRUE(std::holds_alternative<slipstream::codec::NewOrderMessage>(outbound[0].message));
    ASSERT_TRUE(std::holds_alternative<slipstream::codec::ExecReportMessage>(outbound[1].message));
    ASSERT_TRUE(std::holds_alternative<slipstream::codec::ExecReportMessage>(outbound[2].message));

    const auto& order = std::get<slipstream::codec::NewOrderMessage>(outbound[0].message);
    EXPECT_EQ(order.trade_id, 77);
    EXPECT_EQ(order.status, slipstream::codec::NewOrderStatus::accepted);
    EXPECT_EQ(order.side, slipstream::codec::OrderSide::buy);
    EXPECT_EQ(order.qty, 100U);
    EXPECT_TRUE(outbound[0].measure_tick_to_order);
    EXPECT_EQ(outbound[0].trigger_received_at_ns, 100U);

    const auto& ack = std::get<slipstream::codec::ExecReportMessage>(outbound[1].message);
    const auto& fill = std::get<slipstream::codec::ExecReportMessage>(outbound[2].message);
    EXPECT_EQ(ack.status, slipstream::codec::ExecStatus::ack);
    EXPECT_EQ(fill.status, slipstream::codec::ExecStatus::fill);
    EXPECT_EQ(fill.filled_qty, 100U);
    EXPECT_EQ(fill.avg_px, 997'000);

    const ExecutionReport& report = engine.GetExecutionReport();
    EXPECT_EQ(report.market_qty, 10'000U);
    EXPECT_EQ(report.submitted_qty, 100U);
    EXPECT_EQ(report.executed_qty, 100U);
    EXPECT_EQ(report.buy_qty, 100U);
    EXPECT_EQ(report.sell_qty, 0U);
    EXPECT_GE(notifications.load(std::memory_order_relaxed), 1U);
}

TEST_P(EngineModes, AcceptsNotificationAfterDrainingIngressAndStopsWhenIdle) {
    SlipstreamConfig config{};
    config.execution_mode = GetParam();
    config.vwap_window_ms = 1'000;
    config.participation_cap = 1.0;
    config.band_bps = 0.0;
    slipstream::MarketEventQueue ingress;
    slipstream::OrderEntryQueue egress;
    std::atomic<std::uint64_t> generation{0};
    Engine engine{config, ingress, egress, generation, [] {}};
    ASSERT_TRUE(ingress.push({.message = MakeQuote(0, 1'000'000), .received_at_ns = 1}));
    for (std::uint64_t index = 0; index < 12; ++index) {
        ASSERT_TRUE(ingress.push({
            .message = MakeQuote(index == 0 ? 0 : 1'000'000'000ULL, 1'000'100 + static_cast<std::int64_t>(index) * 100),
            .received_at_ns = index + 2,
        }));
    }
    std::jthread worker{[&] { engine.Run(); }};

    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    const slipstream::InboundEvent inbound{.message = MakeTrade(), .received_at_ns = 123};
    const bool pushed = ingress.push(inbound);
    generation.fetch_add(1, std::memory_order_release);
    generation.notify_one();

    slipstream::OutboundMessage message{};
    bool received = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (!(received = egress.pop(message)) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    engine.Stop();
    worker.join();
    EXPECT_TRUE(pushed);
    ASSERT_TRUE(received);
    EXPECT_TRUE(std::holds_alternative<slipstream::codec::NewOrderMessage>(message.message));
}

INSTANTIATE_TEST_SUITE_P(AllPolicies, EngineModes,
                         ::testing::Values(ExecutionMode::Wait, ExecutionMode::Spin, ExecutionMode::Probe));

TEST(ExecutionMode, ParsesNamesAndRejectsUnknownMode) {
    for (auto mode : {ExecutionMode::Wait, ExecutionMode::Spin, ExecutionMode::Probe}) {
        EXPECT_EQ(ParseExecutionMode(ExecutionModeName(mode)), mode);
    }
    EXPECT_EQ(SlipstreamConfig{}.execution_mode, ExecutionMode::Wait);
    EXPECT_THROW(static_cast<void>(ParseExecutionMode("typo")), std::invalid_argument);
}

} // namespace
